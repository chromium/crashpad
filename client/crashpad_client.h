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

#ifndef CRASHPAD_CLIENT_CRASHPAD_CLIENT_H_
#define CRASHPAD_CLIENT_CRASHPAD_CLIENT_H_

#include <map>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "build/build_config.h"

#if defined(OS_MACOSX)
#include "base/mac/scoped_mach_port.h"
#elif defined(OS_WIN)
#include <windows.h>
#endif

namespace crashpad {

//! \brief The primary interface for an application to have Crashpad monitor
//!     it for crashes.
class CrashpadClient {
 public:
  CrashpadClient();
  ~CrashpadClient();

  //! \brief Starts a Crashpad handler process, performing any necessary
  //!     handshake to configure it.
  //!
  //! This method does not actually direct any crashes to the Crashpad handler,
  //! because there are alternative ways to use an existing Crashpad handler. To
  //! begin directing crashes to the handler started by this method, call
  //! UseHandler() after this method returns successfully.
  //!
  //! On Mac OS X, this method starts a Crashpad handler and obtains a Mach
  //! send right corresponding to a receive right held by the handler process.
  //! The handler process runs an exception server on this port.
  //!
  //! \param[in] handler The path to a Crashpad handler executable.
  //! \param[in] database The path to a Crashpad database. The handler will be
  //!     started with this path as its `--database` argument.
  //! \param[in] url The URL of an upload server. The handler will be started
  //!     with this URL as its `--url` argument.
  //! \param[in] annotations Process annotations to set in each crash report.
  //!     The handler will be started with an `--annotation` argument for each
  //!     element in this map.
  //! \param[in] arguments Additional arguments to pass to the Crashpad handler.
  //!     Arguments passed in other parameters and arguments required to perform
  //!     the handshake are the responsibility of this method, and must not be
  //!     specified in this parameter.
  //! \param[in] restartable If `true`, the handler will be restarted if it
  //!     dies, if this behavior is supported. This option is not available on
  //!     all platforms, and does not function on all OS versions. If it is
  //!     not supported, it will be ignored.
  //!
  //! \return `true` on success, `false` on failure with a message logged.
  bool StartHandler(const base::FilePath& handler,
                    const base::FilePath& database,
                    const std::string& url,
                    const std::map<std::string, std::string>& annotations,
                    const std::vector<std::string>& arguments,
                    bool restartable);

#if defined(OS_MACOSX) || DOXYGEN
  //! \brief Sets the process’ crash handler to a Mach service registered with
  //!     the bootstrap server.
  //!
  //! This method does not actually direct any crashes to the Crashpad handler,
  //! because there are alternative ways to start or use an existing Crashpad
  //! handler. To begin directing crashes to the handler set by this method,
  //! call UseHandler() after this method returns successfully.
  //!
  //! This method is only defined on OS X.
  //!
  //! \param[in] service_name The service name of a Crashpad exception handler
  //!     service previously registered with the bootstrap server.
  //!
  //! \return `true` on success, `false` on failure with a message logged.
  bool SetHandlerMachService(const std::string& service_name);

  //! \brief Sets the process’ crash handler to a Mach port.
  //!
  //! This method does not actually direct any crashes to the Crashpad handler,
  //! because there are alternative ways to start or use an existing Crashpad
  //! handler. To begin directing crashes to the handler set by this method,
  //! call UseHandler() after this method.
  //!
  //! This method is only defined on OS X.
  //!
  //! \param[in] exception_port An `exception_port_t` corresponding to a
  //!     Crashpad exception handler service.
  void SetHandlerMachPort(base::mac::ScopedMachSendRight exception_port);
#endif

#if defined(OS_WIN) || DOXYGEN
  //! \brief Sets the IPC pipe of a presumably-running Crashpad handler process
  //!     which was started with StartHandler() or by other compatible means
  //!     and does an IPC message exchange to register this process with the
  //!     handler. However, just like StartHandler(), crashes are not serviced
  //!     until UseHandler() is called.
  //!
  //! This method is only defined on Windows.
  //!
  //! \param[in] ipc_pipe The full name of the crash handler IPC pipe. This is
  //!     a string of the form `&quot;\\.\pipe\NAME&quot;`.
  //!
  //! \return `true` on success and `false` on failure.
  bool SetHandlerIPCPipe(const std::wstring& ipc_pipe);

  //! \brief Retrieves the IPC pipe name used to register with the Crashpad
  //!     handler.
  //!
  //! This method retrieves the IPC pipe name set by SetHandlerIPCPipe(), or a
  //! suitable IPC pipe name chosen by StartHandler(). It is intended to be used
  //! to obtain the IPC pipe name so that it may be passed to other processes,
  //! so that they may register with an existing Crashpad handler by calling
  //! SetHandlerIPCPipe().
  //!
  //! This method is only defined on Windows.
  //!
  //! \return The full name of the crash handler IPC pipe, a string of the form
  //!     `&quot;\\.\pipe\NAME&quot;`.
  std::wstring GetHandlerIPCPipe() const;

  //! \brief Requests that the handler capture a dump even though there hasn't
  //!     been a crash.
  //!
  //! \param[in] context A `CONTEXT`, generally captured by CaptureContext() or
  //!     similar.
  static void DumpWithoutCrash(const CONTEXT& context);

  //! \brief Requests that the handler capture a dump using the given \a
  //!     exception_pointers to get the `EXCEPTION_RECORD` and `CONTEXT`.
  //!
  //! This function is not necessary in general usage as an unhandled exception
  //! filter is installed by UseHandler().
  //!
  //! \param[in] exception_pointers An `EXCEPTION_POINTERS`, as would generally
  //!     passed to an unhandled exception filter.
  static void DumpAndCrash(EXCEPTION_POINTERS* exception_pointers);
#endif

  //! \brief Configures the process to direct its crashes to a Crashpad handler.
  //!
  //! The Crashpad handler must previously have been started by StartHandler()
  //! or configured by SetHandlerMachService(), SetHandlerMachPort(), or
  //! SetHandlerIPCPipe().
  //!
  //! On Mac OS X, this method sets the task’s exception port for `EXC_CRASH`,
  //! `EXC_RESOURCE`, and `EXC_GUARD` exceptions to the Mach send right obtained
  //! by StartHandler(). The handler will be installed with behavior
  //! `EXCEPTION_STATE_IDENTITY | MACH_EXCEPTION_CODES` and thread state flavor
  //! `MACHINE_THREAD_STATE`. Exception ports are inherited, so a Crashpad
  //! handler chosen by UseHandler() will remain the handler for any child
  //! processes created after UseHandler() is called. Child processes do not
  //! need to call StartHandler() or UseHandler() or be aware of Crashpad in any
  //! way. The Crashpad handler will receive crashes from child processes that
  //! have inherited it as their exception handler even after the process that
  //! called StartHandler() exits.
  //!
  //! On Windows, this method sets the unhandled exception handler to a local
  //! function that when reached will "signal and wait" for the crash handler
  //! process to create the dump.
  //!
  //! \return `true` on success, `false` on failure with a message logged.
  bool UseHandler();

#if defined(OS_MACOSX) || DOXYGEN
  //! \brief Configures the process to direct its crashes to the default handler
  //!     for the operating system.
  //!
  //! On OS X, this sets the task’s exception port as in UseHandler(), but the
  //! exception handler used is obtained from SystemCrashReporterHandler(). If
  //! the system’s crash reporter handler cannot be determined or set, the
  //! task’s exception ports for crash-type exceptions are cleared.
  //!
  //! Use of this function is strongly discouraged.
  //!
  //! \warning After a call to this function, Crashpad will no longer monitor
  //!     the process for crashes until a subsequent call to UseHandler().
  //!
  //! \note This is provided as a static function to allow it to be used in
  //!     situations where a CrashpadClient object is not otherwise available.
  //!     This may be useful when a child process inherits its parent’s Crashpad
  //!     handler, but wants to sever this tie.
  static void UseSystemDefaultHandler();
#endif

 private:
#if defined(OS_MACOSX)
  base::mac::ScopedMachSendRight exception_port_;
#elif defined(OS_WIN)
  std::wstring ipc_pipe_;
#endif

  DISALLOW_COPY_AND_ASSIGN(CrashpadClient);
};

}  // namespace crashpad

#endif  // CRASHPAD_CLIENT_CRASHPAD_CLIENT_H_
