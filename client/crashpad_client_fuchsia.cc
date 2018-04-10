// Copyright 2017 The Crashpad Authors. All rights reserved.
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

#include "client/crashpad_client.h"

#include <launchpad/launchpad.h>
#include <zircon/process.h>
#include <zircon/syscalls.h>

#include "base/fuchsia/fuchsia_logging.h"
#include "base/logging.h"
#include "base/scoped_generic.h"
#include "base/strings/stringprintf.h"
#include "client/client_argv_handling.h"
#include "util/fuchsia/exception_information.h"
#include "util/misc/from_pointer_cast.h"

namespace crashpad {

namespace {

std::string FormatArgumentAddress(const std::string& name, void* addr) {
  return base::StringPrintf("--%s=%p", name.c_str(), addr);
}

// Launches a single use handler to snapshot this process.
class LaunchAtCrashHandler {
 public:
  static LaunchAtCrashHandler* Get() {
    static LaunchAtCrashHandler* instance = new LaunchAtCrashHandler();
    return instance;
  }

  bool Initialize(std::vector<std::string>* argv_in) {
    argv_strings_.swap(*argv_in);
    ConvertArgvStrings(argv_strings_, false, &argv_);
    // TODO(scottmg): Install exception handlers or figure out what will do so.
    argv_strings_.push_back(FormatArgumentAddress("parent-exception-address",
                                                  &exception_information_));
    return true;
  }

  bool HandleCrashNonFatal(void* context) {
    exception_information_.context_address =
        FromPointerCast<decltype(exception_information_.context_address)>(
            context);
    exception_information_.thread_id = zx_thread_self();

    launchpad_t* lp;
    launchpad_create(zx_job_default(), argv_[0], &lp);
    launchpad_load_from_file(lp, argv_[0]);
    launchpad_set_args(lp, argv_.size(), &argv_[0]);
    launchpad_clone(
        lp, LP_CLONE_FDIO_ALL | LP_CLONE_ENVIRON | LP_CLONE_DEFAULT_JOB);
    zx_handle_t child;
    const char* error_message;
    zx_status_t status = launchpad_go(lp, &child, &error_message);
    if (status != ZX_OK) {
      ZX_LOG(ERROR, status) << "launchpad_go: " << error_message;
      return false;
    }

    zx_signals_t signals;
    status = zx_object_wait_one(
        child, ZX_TASK_TERMINATED, ZX_TIME_INFINITE, &signals);
    if (status != ZX_OK) {
      ZX_LOG(ERROR, status) << "zx_object_wait_one";
      return false;
    }

    if (signals != ZX_TASK_TERMINATED) {
      LOG(ERROR) << "zx_object_wait_one did not signal ZX_TASK_TERMINATED";
      return false;
    }

    zx_info_process_t proc_info;
    status = zx_object_get_info(child,
                                ZX_INFO_PROCESS,
                                &proc_info,
                                sizeof(proc_info),
                                nullptr,
                                nullptr);
    if (status != ZX_OK) {
      ZX_LOG(ERROR, status) << "zx_object_get_info";
      return false;
    }

    if (proc_info.return_code != 0) {
      LOG(ERROR) << "handler did not exit with code 0";
    }
    return proc_info.return_code == 0;
  }

 private:
  LaunchAtCrashHandler() = default;

  ~LaunchAtCrashHandler() = delete;

  std::vector<std::string> argv_strings_;
  std::vector<const char*> argv_;
  ExceptionInformation exception_information_;

  DISALLOW_COPY_AND_ASSIGN(LaunchAtCrashHandler);
};

static LaunchAtCrashHandler* g_crash_handler;

}  // namespace

CrashpadClient::CrashpadClient() {}

CrashpadClient::~CrashpadClient() {}

bool CrashpadClient::StartHandler(
    const base::FilePath& handler,
    const base::FilePath& database,
    const base::FilePath& metrics_dir,
    const std::string& url,
    const std::map<std::string, std::string>& annotations,
    const std::vector<std::string>& arguments,
    bool restartable,
    bool asynchronous_start) {
  NOTREACHED();  // TODO(scottmg): https://crashpad.chromium.org/bug/196
  return false;
}

// static
bool CrashpadClient::StartHandlerAtCrash(
    const base::FilePath& handler,
    const base::FilePath& database,
    const base::FilePath& metrics_dir,
    const std::string& url,
    const std::map<std::string, std::string>& annotations,
    const std::vector<std::string>& arguments) {

  std::vector<std::string> argv;
  BuildHandlerArgvStrings(
      handler, database, metrics_dir, url, annotations, arguments, &argv);

  auto crash_handler = LaunchAtCrashHandler::Get();
  if (crash_handler->Initialize(&argv)) {
    DCHECK(!g_crash_handler);
    g_crash_handler = crash_handler;
    return true;
  }
  return false;
}

// static
void CrashpadClient::DumpWithoutCrash(NativeCPUContext* context) {
  DCHECK(g_crash_handler);

#if defined(ARCH_CPU_X86_64)
  memset(&context->__fpregs_mem, 0, sizeof(context->__fpregs_mem));
#elif defined(ARCH_CPU_ARM64)
  memset(context->uc_mcontext.__reserved,
         0,
         sizeof(context->uc_mcontext.__reserved));
#else
#error Port.
#endif

  g_crash_handler->HandleCrashNonFatal(reinterpret_cast<void*>(context));
}

}  // namespace crashpad
