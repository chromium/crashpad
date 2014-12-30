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
  //! \param[in] handler The path to a Crashpad handler executable.
  //! \param[in] handler_arguments Arguments to pass to the Crashpad handler.
  //!     Arguments required to perform the handshake are the responsibility of
  //!     this method, and must not be specified in this parameter.
  //!
  //! \return `true` on success, `false` on failure with a message logged.
  bool StartHandler(const base::FilePath& handler,
                    const std::vector<std::string>& handler_arguments);

  //! \brief Configures the process to direct its crashes to a Crashpad handler.
  //!
  //! The Crashpad handler must previously have been started by StartHandler().
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
