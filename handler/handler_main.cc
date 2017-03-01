// Copyright 2014 The Crashpad Authors. All rights reserved.
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

#include "handler/handler_main.h"

#include <errno.h>
#include <getopt.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/auto_reset.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/metrics/persistent_histogram_allocator.h"
#include "base/scoped_generic.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "client/crash_report_database.h"
#include "client/crashpad_client.h"
#include "client/prune_crash_reports.h"
#include "handler/crash_report_upload_thread.h"
#include "handler/prune_crash_reports_thread.h"
#include "tools/tool_support.h"
#include "util/file/file_io.h"
#include "util/misc/metrics.h"
#include "util/numeric/in_range_cast.h"
#include "util/stdlib/map_insert.h"
#include "util/stdlib/string_number_conversion.h"
#include "util/string/split_string.h"
#include "util/synchronization/semaphore.h"

#if defined(OS_MACOSX)
#include <libgen.h>
#include <signal.h>

#include "base/mac/scoped_mach_port.h"
#include "handler/mac/crash_report_exception_handler.h"
#include "handler/mac/exception_handler_server.h"
#include "util/mach/child_port_handshake.h"
#include "util/mach/mach_extensions.h"
#include "util/posix/close_stdio.h"
#include "util/posix/signals.h"
#elif defined(OS_WIN)
#include <windows.h>

#include "handler/win/crash_report_exception_handler.h"
#include "util/win/exception_handler_server.h"
#include "util/win/handle.h"
#include "util/win/initial_client_data.h"
#include "util/win/session_end_watcher.h"
#endif  // OS_MACOSX

namespace crashpad {

namespace {

void Usage(const base::FilePath& me) {
  fprintf(stderr,
"Usage: %" PRFilePath " [OPTION]...\n"
"Crashpad's exception handler server.\n"
"\n"
"      --annotation=KEY=VALUE  set a process annotation in each crash report\n"
"      --database=PATH         store the crash report database at PATH\n"
#if defined(OS_MACOSX)
"      --handshake-fd=FD       establish communication with the client over FD\n"
"      --mach-service=SERVICE  register SERVICE with the bootstrap server\n"
#elif defined(OS_WIN)
"      --initial-client-data=HANDLE_request_crash_dump,\n"
"                            HANDLE_request_non_crash_dump,\n"
"                            HANDLE_non_crash_dump_completed,\n"
"                            HANDLE_pipe,\n"
"                            HANDLE_client_process,\n"
"                            Address_crash_exception_information,\n"
"                            Address_non_crash_exception_information,\n"
"                            Address_debug_critical_section\n"
"                              use precreated data to register initial client\n"
#endif  // OS_MACOSX
"      --metrics-dir=DIR       store metrics files in DIR (only in Chromium)\n"
"      --no-rate-limit         don't rate limit crash uploads\n"
"      --no-upload-gzip        don't use gzip compression when uploading\n"
#if defined(OS_MACOSX)
"      --reset-own-crash-exception-port-to-system-default\n"
"                              reset the server's exception handler to default\n"
#elif defined(OS_WIN)
"      --pipe-name=PIPE        communicate with the client over PIPE\n"
#endif  // OS_MACOSX
"      --url=URL               send crash reports to this Breakpad server URL,\n"
"                              only if uploads are enabled for the database\n"
"      --help                  display this help and exit\n"
"      --version               output version information and exit\n",
          me.value().c_str());
  ToolSupport::UsageTail(me);
}

// Calls Metrics::HandlerLifetimeMilestone, but only on the first call. This is
// to prevent multiple exit events from inadvertently being recorded, which
// might happen if a crash occurs during destruction in what would otherwise be
// a normal exit, or if a CallMetricsRecordNormalExit object is destroyed after
// something else logs an exit event.
void MetricsRecordExit(Metrics::LifetimeMilestone milestone) {
  static bool once = [](Metrics::LifetimeMilestone milestone) {
    Metrics::HandlerLifetimeMilestone(milestone);
    return true;
  }(milestone);
  ALLOW_UNUSED_LOCAL(once);
}

// Calls MetricsRecordExit() to record a failure, and returns EXIT_FAILURE for
// the convenience of callers in main() which can simply write “return
// ExitFailure();”.
int ExitFailure() {
  MetricsRecordExit(Metrics::LifetimeMilestone::kFailed);
  return EXIT_FAILURE;
}

class CallMetricsRecordNormalExit {
 public:
  CallMetricsRecordNormalExit() {}
  ~CallMetricsRecordNormalExit() {
    MetricsRecordExit(Metrics::LifetimeMilestone::kExitedNormally);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CallMetricsRecordNormalExit);
};

#if defined(OS_MACOSX)

void HandleCrashSignal(int sig, siginfo_t* siginfo, void* context) {
  MetricsRecordExit(Metrics::LifetimeMilestone::kCrashed);

  // Is siginfo->si_code useful? The only interesting values on macOS are 0 (not
  // useful, signals generated asynchronously such as by kill() or raise()) and
  // small positive numbers (useful, signal generated via a hardware fault). The
  // standard specifies these other constants, and while xnu never uses them,
  // they are intended to denote signals generated asynchronously and are
  // included here. Additionally, existing practice on other systems
  // (acknowledged by the standard) is for negative numbers to indicate that a
  // signal was generated asynchronously. Although xnu does not do this, allow
  // for the possibility for completeness.
  bool si_code_valid = !(siginfo->si_code <= 0 ||
                         siginfo->si_code == SI_USER ||
                         siginfo->si_code == SI_QUEUE ||
                         siginfo->si_code == SI_TIMER ||
                         siginfo->si_code == SI_ASYNCIO ||
                         siginfo->si_code == SI_MESGQ);

  // 0x5343 = 'SC', signifying “signal and code”, disambiguates from the schema
  // used by ExceptionCodeForMetrics(). That system primarily uses Mach
  // exception types and codes, which are not available to a POSIX signal
  // handler. It does provide a way to encode only signal numbers, but does so
  // with the understanding that certain “raw” signals would not be encountered
  // without a Mach exception. Furthermore, it does not allow siginfo->si_code
  // to be encoded, because that’s not available to Mach exception handlers. It
  // would be a shame to lose that information available to a POSIX signal
  // handler.
  int metrics_code = 0x53430000 | (InRangeCast<uint8_t>(sig, 0xff) << 8);
  if (si_code_valid) {
    metrics_code |= InRangeCast<uint8_t>(siginfo->si_code, 0xff);
  }
  Metrics::HandlerCrashed(metrics_code);

  Signals::RestoreHandlerAndReraiseSignalOnReturn(siginfo, nullptr);
}

void HandleTerminateSignal(int sig, siginfo_t* siginfo, void* context) {
  MetricsRecordExit(Metrics::LifetimeMilestone::kTerminated);
  Signals::RestoreHandlerAndReraiseSignalOnReturn(siginfo, nullptr);
}

void InstallCrashHandler() {
  Signals::InstallCrashHandlers(HandleCrashSignal, 0, nullptr);

  // Not a crash handler, but close enough.
  Signals::InstallTerminateHandlers(HandleTerminateSignal, 0, nullptr);
}

struct ResetSIGTERMTraits {
  static struct sigaction* InvalidValue() {
    return nullptr;
  }

  static void Free(struct sigaction* sa) {
    int rv = sigaction(SIGTERM, sa, nullptr);
    PLOG_IF(ERROR, rv != 0) << "sigaction";
  }
};
using ScopedResetSIGTERM =
    base::ScopedGeneric<struct sigaction*, ResetSIGTERMTraits>;

ExceptionHandlerServer* g_exception_handler_server;

// This signal handler is only operative when being run from launchd.
void HandleSIGTERM(int sig, siginfo_t* siginfo, void* context) {
  // Don’t call MetricsRecordExit(). This is part of the normal exit path when
  // running from launchd.

  DCHECK(g_exception_handler_server);
  g_exception_handler_server->Stop();
}

#elif defined(OS_WIN)

LONG(WINAPI* g_original_exception_filter)(EXCEPTION_POINTERS*) = nullptr;

LONG WINAPI UnhandledExceptionHandler(EXCEPTION_POINTERS* exception_pointers) {
  MetricsRecordExit(Metrics::LifetimeMilestone::kCrashed);
  Metrics::HandlerCrashed(exception_pointers->ExceptionRecord->ExceptionCode);

  if (g_original_exception_filter)
    return g_original_exception_filter(exception_pointers);
  else
    return EXCEPTION_CONTINUE_SEARCH;
}

// Handles events like Control-C and Control-Break on a console.
BOOL WINAPI ConsoleHandler(DWORD console_event) {
  MetricsRecordExit(Metrics::LifetimeMilestone::kTerminated);
  return false;
}

// Handles a WM_ENDSESSION message sent when the user session is ending.
class TerminateHandler final : public SessionEndWatcher {
 public:
  TerminateHandler() : SessionEndWatcher() {}
  ~TerminateHandler() override {}

 private:
  // SessionEndWatcher:
  void SessionEnding() override {
    MetricsRecordExit(Metrics::LifetimeMilestone::kTerminated);
  }

  DISALLOW_COPY_AND_ASSIGN(TerminateHandler);
};

void InstallCrashHandler() {
  g_original_exception_filter =
      SetUnhandledExceptionFilter(&UnhandledExceptionHandler);

  // These are termination handlers, not crash handlers, but that’s close
  // enough. Note that destroying the TerminateHandler would wait for its thread
  // to exit, which isn’t necessary or desirable.
  SetConsoleCtrlHandler(ConsoleHandler, true);
  static TerminateHandler* terminate_handler = new TerminateHandler();
  ALLOW_UNUSED_LOCAL(terminate_handler);
}

#endif  // OS_MACOSX

}  // namespace

int HandlerMain(int argc, char* argv[]) {
  InstallCrashHandler();
  CallMetricsRecordNormalExit metrics_record_normal_exit;

  const base::FilePath argv0(
      ToolSupport::CommandLineArgumentToFilePathStringType(argv[0]));
  const base::FilePath me(argv0.BaseName());

  enum OptionFlags {
    // Long options without short equivalents.
    kOptionLastChar = 255,
    kOptionAnnotation,
    kOptionDatabase,
#if defined(OS_MACOSX)
    kOptionHandshakeFD,
#endif  // OS_MACOSX
#if defined(OS_WIN)
    kOptionInitialClientData,
#endif  // OS_WIN
#if defined(OS_MACOSX)
    kOptionMachService,
#endif  // OS_MACOSX
    kOptionMetrics,
    kOptionNoRateLimit,
    kOptionNoUploadGzip,
#if defined(OS_MACOSX)
    kOptionResetOwnCrashExceptionPortToSystemDefault,
#elif defined(OS_WIN)
    kOptionPipeName,
#endif  // OS_MACOSX
    kOptionURL,

    // Standard options.
    kOptionHelp = -2,
    kOptionVersion = -3,
  };

  struct {
    std::map<std::string, std::string> annotations;
    std::string url;
    const char* database;
    const char* metrics;
#if defined(OS_MACOSX)
    int handshake_fd;
    std::string mach_service;
    bool reset_own_crash_exception_port_to_system_default;
#elif defined(OS_WIN)
    std::string pipe_name;
    InitialClientData initial_client_data;
#endif  // OS_MACOSX
    bool rate_limit;
    bool upload_gzip;
  } options = {};
#if defined(OS_MACOSX)
  options.handshake_fd = -1;
#endif
  options.rate_limit = true;
  options.upload_gzip = true;

  const option long_options[] = {
    {"annotation", required_argument, nullptr, kOptionAnnotation},
    {"database", required_argument, nullptr, kOptionDatabase},
#if defined(OS_MACOSX)
    {"handshake-fd", required_argument, nullptr, kOptionHandshakeFD},
#endif  // OS_MACOSX
#if defined(OS_WIN)
    {"initial-client-data",
     required_argument,
     nullptr,
     kOptionInitialClientData},
#endif  // OS_MACOSX
#if defined(OS_MACOSX)
    {"mach-service", required_argument, nullptr, kOptionMachService},
#endif  // OS_MACOSX
    {"metrics-dir", required_argument, nullptr, kOptionMetrics},
    {"no-rate-limit", no_argument, nullptr, kOptionNoRateLimit},
    {"no-upload-gzip", no_argument, nullptr, kOptionNoUploadGzip},
#if defined(OS_MACOSX)
    {"reset-own-crash-exception-port-to-system-default",
     no_argument,
     nullptr,
     kOptionResetOwnCrashExceptionPortToSystemDefault},
#elif defined(OS_WIN)
    {"pipe-name", required_argument, nullptr, kOptionPipeName},
#endif  // OS_MACOSX
    {"url", required_argument, nullptr, kOptionURL},
    {"help", no_argument, nullptr, kOptionHelp},
    {"version", no_argument, nullptr, kOptionVersion},
    {nullptr, 0, nullptr, 0},
  };

  int opt;
  while ((opt = getopt_long(argc, argv, "", long_options, nullptr)) != -1) {
    switch (opt) {
      case kOptionAnnotation: {
        std::string key;
        std::string value;
        if (!SplitStringFirst(optarg, '=', &key, &value)) {
          ToolSupport::UsageHint(me, "--annotation requires KEY=VALUE");
          return ExitFailure();
        }
        std::string old_value;
        if (!MapInsertOrReplace(&options.annotations, key, value, &old_value)) {
          LOG(WARNING) << "duplicate key " << key << ", discarding value "
                       << old_value;
        }
        break;
      }
      case kOptionDatabase: {
        options.database = optarg;
        break;
      }
#if defined(OS_MACOSX)
      case kOptionHandshakeFD: {
        if (!StringToNumber(optarg, &options.handshake_fd) ||
            options.handshake_fd < 0) {
          ToolSupport::UsageHint(me,
                                 "--handshake-fd requires a file descriptor");
          return ExitFailure();
        }
        break;
      }
      case kOptionMachService: {
        options.mach_service = optarg;
        break;
      }
#elif defined(OS_WIN)
      case kOptionInitialClientData: {
        if (!options.initial_client_data.InitializeFromString(optarg)) {
          ToolSupport::UsageHint(
              me, "failed to parse --initial-client-data");
          return ExitFailure();
        }
        break;
      }
#endif  // OS_MACOSX
      case kOptionMetrics: {
        options.metrics = optarg;
        break;
      }
      case kOptionNoRateLimit: {
        options.rate_limit = false;
        break;
      }
      case kOptionNoUploadGzip: {
        options.upload_gzip = false;
        break;
      }
#if defined(OS_MACOSX)
      case kOptionResetOwnCrashExceptionPortToSystemDefault: {
        options.reset_own_crash_exception_port_to_system_default = true;
        break;
      }
#elif defined(OS_WIN)
      case kOptionPipeName: {
        options.pipe_name = optarg;
        break;
      }
#endif  // OS_MACOSX
      case kOptionURL: {
        options.url = optarg;
        break;
      }
      case kOptionHelp: {
        Usage(me);
        MetricsRecordExit(Metrics::LifetimeMilestone::kExitedEarly);
        return EXIT_SUCCESS;
      }
      case kOptionVersion: {
        ToolSupport::Version(me);
        MetricsRecordExit(Metrics::LifetimeMilestone::kExitedEarly);
        return EXIT_SUCCESS;
      }
      default: {
        ToolSupport::UsageHint(me, nullptr);
        return ExitFailure();
      }
    }
  }
  argc -= optind;
  argv += optind;

#if defined(OS_MACOSX)
  if (options.handshake_fd < 0 && options.mach_service.empty()) {
    ToolSupport::UsageHint(me, "--handshake-fd or --mach-service is required");
    return ExitFailure();
  }
  if (options.handshake_fd >= 0 && !options.mach_service.empty()) {
    ToolSupport::UsageHint(
        me, "--handshake-fd and --mach-service are incompatible");
    return ExitFailure();
  }
#elif defined(OS_WIN)
  if (!options.initial_client_data.IsValid() && options.pipe_name.empty()) {
    ToolSupport::UsageHint(me,
                           "--initial-client-data or --pipe-name is required");
    return ExitFailure();
  }
  if (options.initial_client_data.IsValid() && !options.pipe_name.empty()) {
    ToolSupport::UsageHint(
        me, "--initial-client-data and --pipe-name are incompatible");
    return ExitFailure();
  }
#endif  // OS_MACOSX

  if (!options.database) {
    ToolSupport::UsageHint(me, "--database is required");
    return ExitFailure();
  }

  if (argc) {
    ToolSupport::UsageHint(me, nullptr);
    return ExitFailure();
  }

#if defined(OS_MACOSX)
  if (options.mach_service.empty()) {
    // Don’t do this when being run by launchd. See launchd.plist(5).
    CloseStdinAndStdout();
  }

  if (options.reset_own_crash_exception_port_to_system_default) {
    CrashpadClient::UseSystemDefaultHandler();
  }

  base::mac::ScopedMachReceiveRight receive_right;

  if (options.handshake_fd >= 0) {
    receive_right.reset(
        ChildPortHandshake::RunServerForFD(
            base::ScopedFD(options.handshake_fd),
            ChildPortHandshake::PortRightType::kReceiveRight));
  } else if (!options.mach_service.empty()) {
    receive_right = BootstrapCheckIn(options.mach_service);
  }

  if (!receive_right.is_valid()) {
    return ExitFailure();
  }

  ExceptionHandlerServer exception_handler_server(
      std::move(receive_right), !options.mach_service.empty());
  base::AutoReset<ExceptionHandlerServer*> reset_g_exception_handler_server(
      &g_exception_handler_server, &exception_handler_server);

  struct sigaction old_sigterm_action;
  ScopedResetSIGTERM reset_sigterm;
  if (!options.mach_service.empty()) {
    // When running from launchd, no no-senders notification could ever be
    // triggered, because launchd maintains a send right to the service. When
    // launchd wants the job to exit, it will send a SIGTERM. See
    // launchd.plist(5).
    //
    // Set up a SIGTERM handler that will call exception_handler_server.Stop().
    // This replaces the HandleTerminateSignal handler for SIGTERM.
    if (Signals::InstallHandler(
            SIGTERM, HandleSIGTERM, 0, &old_sigterm_action)) {
      reset_sigterm.reset(&old_sigterm_action);
    }
  }
#elif defined(OS_WIN)
  // Shut down as late as possible relative to programs we're watching.
  if (!SetProcessShutdownParameters(0x100, SHUTDOWN_NORETRY))
    PLOG(ERROR) << "SetProcessShutdownParameters";

  ExceptionHandlerServer exception_handler_server(!options.pipe_name.empty());

  if (!options.pipe_name.empty()) {
    exception_handler_server.SetPipeName(base::UTF8ToUTF16(options.pipe_name));
  }
#endif  // OS_MACOSX

  base::GlobalHistogramAllocator* histogram_allocator = nullptr;
  if (options.metrics) {
    const base::FilePath metrics_dir(
        ToolSupport::CommandLineArgumentToFilePathStringType(options.metrics));
    static const char kMetricsName[] = "CrashpadMetrics";
    const size_t kMetricsFileSize = 1 << 20;
    if (base::GlobalHistogramAllocator::CreateWithActiveFileInDir(
            metrics_dir, kMetricsFileSize, 0, kMetricsName)) {
      histogram_allocator = base::GlobalHistogramAllocator::Get();
      histogram_allocator->CreateTrackingHistograms(kMetricsName);
    }
  }

  Metrics::HandlerLifetimeMilestone(Metrics::LifetimeMilestone::kStarted);

  std::unique_ptr<CrashReportDatabase> database(CrashReportDatabase::Initialize(
      base::FilePath(ToolSupport::CommandLineArgumentToFilePathStringType(
          options.database))));
  if (!database) {
    return ExitFailure();
  }

  // TODO(scottmg): options.rate_limit should be removed when we have a
  // configurable database setting to control upload limiting.
  // See https://crashpad.chromium.org/bug/23.
  CrashReportUploadThread upload_thread(
      database.get(), options.url, options.rate_limit, options.upload_gzip);
  upload_thread.Start();

  PruneCrashReportThread prune_thread(database.get(),
                                      PruneCondition::GetDefault());
  prune_thread.Start();

  CrashReportExceptionHandler exception_handler(
      database.get(), &upload_thread, &options.annotations);

#if defined(OS_WIN)
  if (options.initial_client_data.IsValid()) {
    exception_handler_server.InitializeWithInheritedDataForInitialClient(
        options.initial_client_data, &exception_handler);
  }
#endif  // OS_WIN

  exception_handler_server.Run(&exception_handler);

  upload_thread.Stop();
  prune_thread.Stop();

  if (histogram_allocator)
    histogram_allocator->DeletePersistentLocation();

  return EXIT_SUCCESS;
}

}  // namespace crashpad
