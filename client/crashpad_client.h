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

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "build/build_config.h"

#if defined(OS_MACOSX)
#include "base/mac/scoped_mach_port.h"
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
  //! because there may be alternative ways to use an existing Crashpad handler
  //! without having to start one. To begin directing crashes to the handler,
  //! started by this method, call UseHandler() after this method returns
  //! successfully.
  //!
  //! On Mac OS X, this method starts a Crashpad handler and obtains a Mach
  //! send right corresponding to a receive right held by the handler process.
  //! The handler process runs an exception server on this port.
  //!
  //! On Windows, SetHandler() is normally used instead since the handler is
  //! started by other means.
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
  //!
  //! \return `true` on success, `false` on failure with a message logged.
  bool StartHandler(const base::FilePath& handler,
                    const base::FilePath& database,
                    const std::string& url,
                    const std::map<std::string, std::string>& annotations,
                    const std::vector<std::string>& arguments);

#if defined(OS_WIN) || DOXYGEN
  //! \brief Sets the IPC port of a presumably-running Crashpad handler process
  //!     which was started with StartHandler() or by other compatible means
  //!     and does an IPC message exchange to register this process with the
  //!     handler. However, just like StartHandler(), crashes are not serviced
  //!     until UseHandler() is called.
  //!
  //! The IPC port name (somehow) encodes enough information so that
  //! registration is done with a crash handler using the appropriate database
  //! and upload server.
  //!
  //! \param[in] ipc_port The full name of the crash handler IPC port.
  //!
  //! \return `true` on success and `false` on failure.
  bool SetHandler(const std::string& ipc_port);
#endif

  //! \brief Configures the process to direct its crashes to a Crashpad handler.
  //!
  //! The Crashpad handler must previously have been started by StartHandler()
  //! or configured by SetHandler().
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

 private:
#if defined(OS_MACOSX)
  base::mac::ScopedMachSendRight exception_port_;
#endif

  DISALLOW_COPY_AND_ASSIGN(CrashpadClient);
};

}  // namespace crashpad

#endif  // CRASHPAD_CLIENT_CRASHPAD_CLIENT_H_
