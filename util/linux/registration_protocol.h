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

#include <stdint.h>
#include <sys/types.h>

#include "util/linux/address_types.h"

namespace crashpad {

#pragma pack(push, 1)

struct Registration {
  LinuxVMAddress exception_information_address;
  pid_t client_process_id;
  bool use_broker;
};

//! \brief Registers
bool RegisterWithHandler(int server_sock,
                         const Registration& registration,
                         int registration_socket);

bool RequestCrashDump(int registration_socket);

//! \brief The message passed from client to server
struct ClientToServerMessage {
  //! \brief Indicates which field of the union is in use.
  enum Type : uint32_t {
    //! \brief For Registration
    kRegistration,

    //! \brief Request a crash dump
    kCrashDumpRequest
  } type;

  union {
    Registration registration;
  };
};

//! \brief The response sent back to the client
union ServerToClientMessage {};

#pragma pack(pop)

}  // namespace crashpad

#endif  // CRASHPAD_UTIL_LINUX_REGISTRATION_PROTOCOL_H_
