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

#include "base/auto_reset.h"
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
#elif defined(OS_WIN)
#include <windows.h>

#include "handler/win/crash_report_exception_handler.h"
#include "util/win/exception_handler_server.h"
#include "util/win/handle.h"
#include "util/win/initial_client_data.h"
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

#if defined(OS_MACOSX)

struct sigaction g_original_crash_sigaction[NSIG];

void HandleCrashSignal(int sig, siginfo_t* siginfo, void* context) {
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

  // Restore the previous signal handler.
  DCHECK_GT(sig, 0);
  DCHECK_LT(static_cast<size_t>(sig), arraysize(g_original_crash_sigaction));
  struct sigaction* osa = &g_original_crash_sigaction[sig];
  int rv = sigaction(sig, osa, nullptr);
  DPLOG_IF(ERROR, rv != 0) << "sigaction " << sig;

  // If the signal was received synchronously resulting from a hardware fault,
  // returning from the signal handler will cause the kernel to re-raise it,
  // because this handler hasn’t done anything to alleviate the condition that
  // caused the signal to be raised in the first place. With the old signal
  // handler in place (expected to be SIG_DFL), it will cause the same behavior
  // to be taken as though this signal handler had never been installed at all
  // (expected to be a crash). This is ideal, because the signal is re-raised
  // with the same properties and from the same context that initially triggered
  // it, providing the best debugging experience.

  if ((sig != SIGILL && sig != SIGFPE && sig != SIGBUS && sig != SIGSEGV) ||
      !si_code_valid) {
    // Signals received other than via hardware faults, such as those raised
    // asynchronously via kill() and raise(), and those arising via hardware
    // traps such as int3 (resulting in SIGTRAP but advancing the instruction
    // pointer), will not reoccur on their own when returning from the signal
    // handler. Re-raise them or call to the previous signal handler as
    // appropriate.
    //
    // Unfortunately, when SIGBUS is received asynchronously via kill(),
    // siginfo->si_code makes it appear as though it was actually received via a
    // hardware fault. See 10.12.3 xnu-3789.41.3/bsd/dev/i386/unix_signal.c
    // sendsig(). An asynchronous SIGBUS will thus cause the handler-crashed
    // metric to be logged but will not cause the process to terminate. This
    // isn’t ideal, but asynchronous SIGBUS is an unexpected condition. The
    // alternative, to re-raise here on any SIGBUS, is a bad idea because it
    // would lose properties associated with the the original signal, which are
    // very valuable for debugging and are visible to a Mach exception handler.
    // Since SIGBUS is normally received synchronously in response to a hardware
    // fault, don’t sweat the unexpected asynchronous case.
    if (osa->sa_handler == SIG_DFL) {
      // Because this signal handler executes with the signal blocked, this
      // raise() cannot immediately deliver the signal. Delivery is deferred
      // until this signal handler returns and the signal becomes unblocked. The
      // re-raised signal will appear with the same context as where it was
      // initially triggered.
      rv = raise(sig);
      DPLOG_IF(ERROR, rv != 0) << "raise";
    } else if (osa->sa_handler != SIG_IGN) {
      if (osa->sa_flags & SA_SIGINFO) {
        osa->sa_sigaction(sig, siginfo, context);
      } else {
        osa->sa_handler(sig);
      }
    }
  }
}

void InstallCrashHandler() {
  struct sigaction sa = {};
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_SIGINFO;
  sa.sa_sigaction = HandleCrashSignal;

  // These are the core-generating signals from 10.12.3
  // xnu-3789.41.3/bsd/sys/signalvar.h sigprop: entries with SA_CORE are in the
  // set.
  const int kSignals[] = {SIGQUIT,
                          SIGILL,
                          SIGTRAP,
                          SIGABRT,
                          SIGEMT,
                          SIGFPE,
                          SIGBUS,
                          SIGSEGV,
                          SIGSYS};

  for (int sig : kSignals) {
    DCHECK_GT(sig, 0);
    DCHECK_LT(static_cast<size_t>(sig), arraysize(g_original_crash_sigaction));
    int rv = sigaction(sig, &sa, &g_original_crash_sigaction[sig]);
    PCHECK(rv == 0) << "sigaction " << sig;
  }
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
  DCHECK(g_exception_handler_server);
  g_exception_handler_server->Stop();
}

#elif defined(OS_WIN)

LONG(WINAPI* g_original_exception_filter)(EXCEPTION_POINTERS*) = nullptr;

LONG WINAPI UnhandledExceptionHandler(EXCEPTION_POINTERS* exception_pointers) {
  Metrics::HandlerCrashed(exception_pointers->ExceptionRecord->ExceptionCode);

  if (g_original_exception_filter)
    return g_original_exception_filter(exception_pointers);
  else
    return EXCEPTION_CONTINUE_SEARCH;
}

void InstallCrashHandler() {
  g_original_exception_filter =
      SetUnhandledExceptionFilter(&UnhandledExceptionHandler);
}

#endif  // OS_MACOSX

}  // namespace

int HandlerMain(int argc, char* argv[]) {
  InstallCrashHandler();

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
  } options = {};
#if defined(OS_MACOSX)
  options.handshake_fd = -1;
#endif
  options.rate_limit = true;

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
#elif defined(OS_WIN)
      case kOptionInitialClientData: {
        if (!options.initial_client_data.InitializeFromString(optarg)) {
          ToolSupport::UsageHint(
              me, "failed to parse --initial-client-data");
          return EXIT_FAILURE;
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
  if (!options.initial_client_data.IsValid() && options.pipe_name.empty()) {
    ToolSupport::UsageHint(me,
                           "--initial-client-data or --pipe-name is required");
    return EXIT_FAILURE;
  }
  if (options.initial_client_data.IsValid() && !options.pipe_name.empty()) {
    ToolSupport::UsageHint(
        me, "--initial-client-data and --pipe-name are incompatible");
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
      std::move(receive_right), !options.mach_service.empty());
  base::AutoReset<ExceptionHandlerServer*> reset_g_exception_handler_server(
      &g_exception_handler_server, &exception_handler_server);

  struct sigaction old_sa;
  ScopedResetSIGTERM reset_sigterm;
  if (!options.mach_service.empty()) {
    // When running from launchd, no no-senders notification could ever be
    // triggered, because launchd maintains a send right to the service. When
    // launchd wants the job to exit, it will send a SIGTERM. See
    // launchd.plist(5).
    //
    // Set up a SIGTERM handler that will call exception_handler_server.Stop().
    struct sigaction sa = {};
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = HandleSIGTERM;
    int rv = sigaction(SIGTERM, &sa, &old_sa);
    PCHECK(rv == 0) << "sigaction";
    reset_sigterm.reset(&old_sa);
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

  std::unique_ptr<CrashReportDatabase> database(CrashReportDatabase::Initialize(
      base::FilePath(ToolSupport::CommandLineArgumentToFilePathStringType(
          options.database))));
  if (!database) {
    return EXIT_FAILURE;
  }

  // TODO(scottmg): options.rate_limit should be removed when we have a
  // configurable database setting to control upload limiting.
  // See https://crashpad.chromium.org/bug/23.
  CrashReportUploadThread upload_thread(
      database.get(), options.url, options.rate_limit);
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
