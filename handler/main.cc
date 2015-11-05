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

#include <getopt.h>
#include <stdlib.h>

#include <map>
#include <string>

#include "base/files/file_path.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "client/crash_report_database.h"
#include "client/crashpad_client.h"
#include "tools/tool_support.h"
#include "handler/crash_report_upload_thread.h"
#include "util/file/file_io.h"
#include "util/stdlib/map_insert.h"
#include "util/stdlib/string_number_conversion.h"
#include "util/string/split_string.h"
#include "util/synchronization/semaphore.h"

#if defined(OS_MACOSX)
#include <libgen.h>
#include "base/mac/scoped_mach_port.h"
#include "handler/mac/crash_report_exception_handler.h"
#include "handler/mac/exception_handler_server.h"
#include "util/mach/child_port_handshake.h"
#include "util/mach/mach_extensions.h"
#include "util/posix/close_stdio.h"
#elif defined(OS_WIN)
#include <windows.h>
#include "handler/win/crash_report_exception_handler.h"
#include "util/win/exception_handler_server.h"
#include "util/win/handle.h"
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
"      --reset-own-crash-exception-port-to-system-default\n"
"                              reset the server's exception handler to default\n"
#elif defined(OS_WIN)
"      --handshake-handle=HANDLE\n"
"                              create a new pipe and send its name via HANDLE\n"
"      --pipe-name=PIPE        communicate with the client over PIPE\n"
#endif  // OS_MACOSX
"      --url=URL               send crash reports to this Breakpad server URL,\n"
"                              only if uploads are enabled for the database\n"
"      --help                  display this help and exit\n"
"      --version               output version information and exit\n",
          me.value().c_str());
  ToolSupport::UsageTail(me);
}

int HandlerMain(int argc, char* argv[]) {
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
    kOptionMachService,
    kOptionResetOwnCrashExceptionPortToSystemDefault,
#elif defined(OS_WIN)
    kOptionHandshakeHandle,
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
#if defined(OS_MACOSX)
    int handshake_fd;
    std::string mach_service;
    bool reset_own_crash_exception_port_to_system_default;
#elif defined(OS_WIN)
    HANDLE handshake_handle;
    std::string pipe_name;
#endif  // OS_MACOSX
  } options = {};
#if defined(OS_MACOSX)
  options.handshake_fd = -1;
#elif defined(OS_WIN)
  options.handshake_handle = INVALID_HANDLE_VALUE;
#endif

  const option long_options[] = {
      {"annotation", required_argument, nullptr, kOptionAnnotation},
      {"database", required_argument, nullptr, kOptionDatabase},
#if defined(OS_MACOSX)
      {"handshake-fd", required_argument, nullptr, kOptionHandshakeFD},
      {"mach-service", required_argument, nullptr, kOptionMachService},
      {"reset-own-crash-exception-port-to-system-default",
       no_argument,
       nullptr,
       kOptionResetOwnCrashExceptionPortToSystemDefault},
#elif defined(OS_WIN)
      {"handshake-handle", required_argument, nullptr, kOptionHandshakeHandle},
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
        if (!SplitString(optarg, '=', &key, &value)) {
          ToolSupport::UsageHint(me, "--annotation requires KEY=VALUE");
          return EXIT_FAILURE;
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
          return EXIT_FAILURE;
        }
        break;
      }
      case kOptionMachService: {
        options.mach_service = optarg;
        break;
      }
      case kOptionResetOwnCrashExceptionPortToSystemDefault: {
        options.reset_own_crash_exception_port_to_system_default = true;
        break;
      }
#elif defined(OS_WIN)
      case kOptionHandshakeHandle: {
        // Use unsigned int, because the handle was presented by the client in
        // 0x%x format.
        unsigned int handle_uint;
        if (!StringToNumber(optarg, &handle_uint) ||
            (options.handshake_handle = IntToHandle(handle_uint)) ==
                INVALID_HANDLE_VALUE) {
          ToolSupport::UsageHint(me, "--handshake-handle requires a HANDLE");
          return EXIT_FAILURE;
        }
        break;
      }
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
        return EXIT_SUCCESS;
      }
      case kOptionVersion: {
        ToolSupport::Version(me);
        return EXIT_SUCCESS;
      }
      default: {
        ToolSupport::UsageHint(me, nullptr);
        return EXIT_FAILURE;
      }
    }
  }
  argc -= optind;
  argv += optind;

#if defined(OS_MACOSX)
  if (options.handshake_fd < 0 && options.mach_service.empty()) {
    ToolSupport::UsageHint(me, "--handshake-fd or --mach-service is required");
    return EXIT_FAILURE;
  }
  if (options.handshake_fd >= 0 && !options.mach_service.empty()) {
    ToolSupport::UsageHint(
        me, "--handshake-fd and --mach-service are incompatible");
    return EXIT_FAILURE;
  }
#elif defined(OS_WIN)
  if (options.handshake_handle == INVALID_HANDLE_VALUE &&
      options.pipe_name.empty()) {
    ToolSupport::UsageHint(me, "--handshake-handle or --pipe-name is required");
    return EXIT_FAILURE;
  }
  if (options.handshake_handle != INVALID_HANDLE_VALUE &&
      !options.pipe_name.empty()) {
    ToolSupport::UsageHint(
        me, "--handshake-handle and --pipe-name are incompatible");
    return EXIT_FAILURE;
  }
#endif  // OS_MACOSX

  if (!options.database) {
    ToolSupport::UsageHint(me, "--database is required");
    return EXIT_FAILURE;
  }

  if (argc) {
    ToolSupport::UsageHint(me, nullptr);
    return EXIT_FAILURE;
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
    return EXIT_FAILURE;
  }

  ExceptionHandlerServer exception_handler_server(
      receive_right.Pass(), !options.mach_service.empty());
#elif defined(OS_WIN)
  ExceptionHandlerServer exception_handler_server(!options.pipe_name.empty());

  std::string pipe_name;
  if (!options.pipe_name.empty()) {
    exception_handler_server.SetPipeName(base::UTF8ToUTF16(options.pipe_name));
  } else if (options.handshake_handle != INVALID_HANDLE_VALUE) {
    std::wstring pipe_name = exception_handler_server.CreatePipe();

    uint32_t pipe_name_length = static_cast<uint32_t>(pipe_name.size());
    if (!LoggingWriteFile(options.handshake_handle,
                          &pipe_name_length,
                          sizeof(pipe_name_length))) {
      return EXIT_FAILURE;
    }
    if (!LoggingWriteFile(options.handshake_handle,
                          pipe_name.c_str(),
                          pipe_name.size() * sizeof(pipe_name[0]))) {
      return EXIT_FAILURE;
    }
  }
#endif  // OS_MACOSX

  scoped_ptr<CrashReportDatabase> database(CrashReportDatabase::Initialize(
      base::FilePath(ToolSupport::CommandLineArgumentToFilePathStringType(
          options.database))));
  if (!database) {
    return EXIT_FAILURE;
  }

  CrashReportUploadThread upload_thread(database.get(), options.url);
  upload_thread.Start();

  CrashReportExceptionHandler exception_handler(
      database.get(), &upload_thread, &options.annotations);

  exception_handler_server.Run(&exception_handler);

  upload_thread.Stop();

  return EXIT_SUCCESS;
}

}  // namespace
}  // namespace crashpad

#if defined(OS_MACOSX)
int main(int argc, char* argv[]) {
  return crashpad::HandlerMain(argc, argv);
}
#elif defined(OS_WIN)
int wmain(int argc, wchar_t* argv[]) {
  return crashpad::ToolSupport::Wmain(argc, argv, crashpad::HandlerMain);
}
#endif  // OS_MACOSX
