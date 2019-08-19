// Copyright 2018 The Crashpad Authors. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "handler/linux/crash_report_exception_handler.h"

#include <vector>

#include "base/logging.h"
#include "client/settings.h"
#include "minidump/minidump_file_writer.h"
#include "snapshot/crashpad_info_client_options.h"
#include "snapshot/linux/process_snapshot_linux.h"
#include "snapshot/sanitized/process_snapshot_sanitized.h"
#include "snapshot/sanitized/sanitization_information.h"
#include "util/linux/direct_ptrace_connection.h"
#include "util/linux/ptrace_client.h"
#include "util/misc/metrics.h"
#include "util/misc/tri_state.h"
#include "util/misc/uuid.h"

#if defined(OS_CHROMEOS)
#include "handler/minidump_to_upload_parameters.h"
#include "snapshot/minidump/process_snapshot_minidump.h"
#include "util/posix/double_fork_and_exec.h"

namespace {
// Returns the process name for a pid.
const std::string GetProcessNameFromPid(pid_t pid) {
  // Symlink to process binary is at /proc/###/exe.
  std::string link_path = "/proc/" + std::to_string(pid) + "/exe";

  constexpr int kMaxSize = 4096;
  std::unique_ptr<char[]> buf(new char[kMaxSize]);
  ssize_t size = readlink(link_path.c_str(), buf.get(), kMaxSize);
  std::string result;
  if (size < 0) {
    PLOG(ERROR) << "Failed to readlink " << link_path;
  } else {
    result.assign(buf.get(), size);
    size_t last_slash_pos = result.rfind('/');
    if (last_slash_pos != std::string::npos) {
      result = result.substr(last_slash_pos + 1);
    }
  }
  return result;
}

bool WriteAnnotationsAndMinidump(
    const std::map<std::string, std::string>& parameters,
    crashpad::MinidumpFileWriter& minidump,
    crashpad::FileWriter& file_writer) {
  for (const auto& kv : parameters) {
    if (kv.first.find(':') != std::string::npos) {
      LOG(ERROR) << "Annotation key cannot have ':' in it " << kv.first;
      return false;
    }
    if (!file_writer.Write(kv.first.c_str(), strlen(kv.first.c_str()))) {
      return false;
    }
    if (!file_writer.Write(":", 1)) {
      return false;
    }
    size_t value_size = strlen(kv.second.c_str());
    std::string value_size_str = std::to_string(value_size);
    if (!file_writer.Write(value_size_str.c_str(), value_size_str.size())) {
      return false;
    }
    if (!file_writer.Write(":", 1)) {
      return false;
    }
    if (!file_writer.Write(kv.second.c_str(), strlen(kv.second.c_str()))) {
      return false;
    }
  }

  static constexpr char kMinidumpName[] =
      "upload_file_minidump\"; filename=\"dump\":";
  if (!file_writer.Write(kMinidumpName, sizeof(kMinidumpName) - 1)) {
    return false;
  }
  crashpad::FileOffset dump_size_start_offset = file_writer.Seek(0, SEEK_CUR);
  if (dump_size_start_offset < 0) {
    LOG(ERROR) << "Failed to get minidump size start offset";
    return false;
  }
  static constexpr char kMinidumpLengthFilling[] = "00000000000000000000:";
  if (!file_writer.Write(kMinidumpLengthFilling,
                         sizeof(kMinidumpLengthFilling) - 1)) {
    return false;
  }
  crashpad::FileOffset dump_start_offset = file_writer.Seek(0, SEEK_CUR);
  if (dump_start_offset < 0) {
    LOG(ERROR) << "Failed to get minidump start offset";
    return false;
  }
  if (!minidump.WriteEverything(&file_writer)) {
    return false;
  }
  crashpad::FileOffset dump_end_offset = file_writer.Seek(0, SEEK_CUR);
  if (dump_end_offset < 0) {
    LOG(ERROR) << "Failed to get minidump end offset";
    return false;
  }

  size_t dump_data_size = dump_end_offset - dump_start_offset;
  std::string dump_data_size_str = std::to_string(dump_data_size);
  file_writer.Seek(dump_size_start_offset + strlen(kMinidumpLengthFilling) - 1 -
                       dump_data_size_str.size(),
                   SEEK_SET);
  if (!file_writer.Write(dump_data_size_str.c_str(),
                         dump_data_size_str.size())) {
    return false;
  }
  return true;
}

}  // namespace
#endif  // defined(OS_CHROMEOS)

namespace crashpad {

CrashReportExceptionHandler::CrashReportExceptionHandler(
    CrashReportDatabase* database,
    CrashReportUploadThread* upload_thread,
    const std::map<std::string, std::string>* process_annotations,
    const UserStreamDataSources* user_stream_data_sources)
    : database_(database),
#if !defined(OS_CHROMEOS)
      upload_thread_(upload_thread),
#endif
      process_annotations_(process_annotations),
      user_stream_data_sources_(user_stream_data_sources) {
}

CrashReportExceptionHandler::~CrashReportExceptionHandler() = default;

bool CrashReportExceptionHandler::HandleException(
    pid_t client_process_id,
    uid_t client_uid,
    const ExceptionHandlerProtocol::ClientInformation& info,
    VMAddress requesting_thread_stack_address,
    pid_t* requesting_thread_id,
    UUID* local_report_id) {
  Metrics::ExceptionEncountered();

  DirectPtraceConnection connection;
  if (!connection.Initialize(client_process_id)) {
    Metrics::ExceptionCaptureResult(
        Metrics::CaptureResult::kDirectPtraceFailed);
    return false;
  }

  return HandleExceptionWithConnection(&connection,
                                       info,
                                       client_uid,
                                       requesting_thread_stack_address,
                                       requesting_thread_id,
                                       local_report_id);
}

bool CrashReportExceptionHandler::HandleExceptionWithBroker(
    pid_t client_process_id,
    uid_t client_uid,
    const ExceptionHandlerProtocol::ClientInformation& info,
    int broker_sock,
    UUID* local_report_id) {
  Metrics::ExceptionEncountered();

  PtraceClient client;
  if (!client.Initialize(broker_sock, client_process_id)) {
    Metrics::ExceptionCaptureResult(
        Metrics::CaptureResult::kBrokeredPtraceFailed);
    return false;
  }

  return HandleExceptionWithConnection(
      &client, info, client_uid, 0, nullptr, local_report_id);
}

bool CrashReportExceptionHandler::HandleExceptionWithConnection(
    PtraceConnection* connection,
    const ExceptionHandlerProtocol::ClientInformation& info,
    uid_t client_uid,
    VMAddress requesting_thread_stack_address,
    pid_t* requesting_thread_id,
    UUID* local_report_id) {
  ProcessSnapshotLinux process_snapshot;
  if (!process_snapshot.Initialize(connection)) {
    Metrics::ExceptionCaptureResult(Metrics::CaptureResult::kSnapshotFailed);
    return false;
  }

  pid_t local_requesting_thread_id = -1;
  if (requesting_thread_stack_address) {
    local_requesting_thread_id = process_snapshot.FindThreadWithStackAddress(
        requesting_thread_stack_address);
  }

  if (requesting_thread_id) {
    *requesting_thread_id = local_requesting_thread_id;
  }

  if (!process_snapshot.InitializeException(info.exception_information_address,
                                            local_requesting_thread_id)) {
    Metrics::ExceptionCaptureResult(
        Metrics::CaptureResult::kExceptionInitializationFailed);
    return false;
  }

  Metrics::ExceptionCode(process_snapshot.Exception()->Exception());

  CrashpadInfoClientOptions client_options;
  process_snapshot.GetCrashpadOptions(&client_options);
  if (client_options.crashpad_handler_behavior != TriState::kDisabled) {
    UUID client_id;
    Settings* const settings = database_->GetSettings();
    if (settings) {
      // If GetSettings() or GetClientID() fails, something else will log a
      // message and client_id will be left at its default value, all zeroes,
      // which is appropriate.
      settings->GetClientID(&client_id);
    }

    process_snapshot.SetClientID(client_id);
    for (auto& p : *process_annotations_) {
      process_snapshot.AddAnnotation(p.first, p.second);
    }

    UUID uuid;
#if defined(OS_CHROMEOS)
    uuid.InitializeWithNew();
    process_snapshot.SetReportID(uuid);
#else

    std::unique_ptr<CrashReportDatabase::NewReport> new_report;
    CrashReportDatabase::OperationStatus database_status =
        database_->PrepareNewCrashReport(&new_report);
    if (database_status != CrashReportDatabase::kNoError) {
      LOG(ERROR) << "PrepareNewCrashReport failed";
      Metrics::ExceptionCaptureResult(
          Metrics::CaptureResult::kPrepareNewCrashReportFailed);
      return false;
    }

    process_snapshot.SetReportID(new_report->ReportID());
#endif

    ProcessSnapshot* snapshot = nullptr;
    ProcessSnapshotSanitized sanitized;
    std::vector<std::string> annotations_whitelist;
    std::vector<std::pair<VMAddress, VMAddress>> memory_range_whitelist;
    if (info.sanitization_information_address) {
      SanitizationInformation sanitization_info;
      ProcessMemoryRange range;
      if (!range.Initialize(connection->Memory(), connection->Is64Bit()) ||
          !range.Read(info.sanitization_information_address,
                      sizeof(sanitization_info),
                      &sanitization_info)) {
        Metrics::ExceptionCaptureResult(
            Metrics::CaptureResult::kSanitizationInitializationFailed);
        return false;
      }

      if (!ReadAnnotationsWhitelist(
              range,
              sanitization_info.annotations_whitelist_address,
              &annotations_whitelist) ||
          !ReadMemoryRangeWhitelist(
              range,
              sanitization_info.memory_range_whitelist_address,
              &memory_range_whitelist)) {
        Metrics::ExceptionCaptureResult(
            Metrics::CaptureResult::kSanitizationInitializationFailed);
        return false;
      }

      if (!sanitized.Initialize(&process_snapshot,
                                sanitization_info.annotations_whitelist_address
                                    ? &annotations_whitelist
                                    : nullptr,
                                &memory_range_whitelist,
                                sanitization_info.target_module_address,
                                sanitization_info.sanitize_stacks)) {
        Metrics::ExceptionCaptureResult(
            Metrics::CaptureResult::kSkippedDueToSanitization);
        return true;
      }

      snapshot = &sanitized;
    } else {
      snapshot = &process_snapshot;
    }

    MinidumpFileWriter minidump;
    minidump.InitializeFromSnapshot(snapshot);
    AddUserExtensionStreams(user_stream_data_sources_, snapshot, &minidump);

#if defined(OS_CHROMEOS)
    FileWriter file_writer;
    if (!file_writer.OpenMemfd(base::FilePath("/tmp/minidump"))) {
      Metrics::ExceptionCaptureResult(Metrics::CaptureResult::kOpenMemfdFailed);
      return false;
    }

    std::map<std::string, std::string> parameters =
        BreakpadHTTPFormParametersFromMinidump(snapshot);
    // Used to differenciate between breakpad and crashpad while the switch is
    // ramping up.
    parameters.emplace("crash_library", "crashpad");

    if (!WriteAnnotationsAndMinidump(parameters, minidump, file_writer)) {
      Metrics::ExceptionCaptureResult(
          Metrics::CaptureResult::kMinidumpWriteFailed);
      return false;
    }

    // CrOS uses crash_reporter instead of Crashpad to report crashes.
    // crash_reporter needs to know the pid and uid of the crashing process.
    std::vector<std::string> argv({"/sbin/crash_reporter"});

    argv.push_back("--chrome_memfd=" + std::to_string(file_writer.fd()));
    argv.push_back("--pid=" + std::to_string(*requesting_thread_id));
    argv.push_back("--uid=" + std::to_string(client_uid));
    std::string process_name = GetProcessNameFromPid(*requesting_thread_id);
    argv.push_back("--exe=" + (process_name.empty() ? "chrome" : process_name));

    if (info.crash_loop_before_time != 0) {
      argv.push_back("--crash_loop_before=" +
                     std::to_string(info.crash_loop_before_time));
    }

    if (!DoubleForkAndExec(argv,
                           nullptr /* envp */,
                           file_writer.fd() /* preserve_fd */,
                           false /* use_path */,
                           nullptr /* child_function */)) {
      LOG(ERROR) << "DoubleForkAndExec failed";
      Metrics::ExceptionCaptureResult(
          Metrics::CaptureResult::kFinishedWritingCrashReportFailed);
      return false;
    }

#else

    if (!minidump.WriteEverything(new_report->Writer())) {
      LOG(ERROR) << "WriteEverything failed";
      Metrics::ExceptionCaptureResult(
          Metrics::CaptureResult::kMinidumpWriteFailed);
      return false;
    }

    database_status =
        database_->FinishedWritingCrashReport(std::move(new_report), &uuid);
    if (database_status != CrashReportDatabase::kNoError) {
      LOG(ERROR) << "FinishedWritingCrashReport failed";
      Metrics::ExceptionCaptureResult(
          Metrics::CaptureResult::kFinishedWritingCrashReportFailed);
      return false;
    }

    if (upload_thread_) {
      upload_thread_->ReportPending(uuid);
    }
#endif
    if (local_report_id != nullptr) {
      *local_report_id = uuid;
    }
  }

  Metrics::ExceptionCaptureResult(Metrics::CaptureResult::kSuccess);
  return true;
}

}  // namespace crashpad
