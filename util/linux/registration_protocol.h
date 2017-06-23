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

#ifndef CRASHPAD_UTIL_LINUX_REGISTRATION_PROTOCOL_H_
#define CRASHPAD_UTIL_LINUX_REGISTRATION_PROTOCOL_H_

#include <sys/types.h>

#include <string>

#include "util/linux/address_types.h"

namespace crashpad {

#pragma pack(push, 1)

//! \brief Structure read out of the client process by the crash handler when an
//!     exception occurs.
struct ExceptionInformation {
  //! \brief The address of the siginfo_t passed to the signal handler in the
  //!     crashed process.
  LinuxVMAddress siginfo;

  //! \brief The thread ID of the thread which received the signal.
  pid_t thread_id;
};

//! \brief A message used to request a dump after a crash.
struct CrashDumpRequest {
  CrashDumpRequest();

  //! \brief Returns `true` if the request appears to contain valid data.
  bool IsValid() const;

  //! \brief Initializes this object from a string representation, presumed to
  //!     have been created by ToString().
  bool InitializeFromString(const std::string& string);

  //! \brief Returns a string representation of this object.
  std::string ToString() const;

  //! \brief The address in the client's address space of an
  //!     ExceptionInformation struct.
  LinuxVMAddress exception_information_address;

  //! \brief The process ID of the client to dump.
  pid_t client_process_id;
};

#pragma pack(pop)

}  // namespace crashpad

#endif  // CRASHPAD_UTIL_LINUX_REGISTRATION_PROTOCOL_H_
