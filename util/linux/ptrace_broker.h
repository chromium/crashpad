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

#ifndef CRASHPAD_UTIL_LINUX_PTRACE_BROKER_H_
#define CRASHPAD_UTIL_LINUX_PTRACE_BROKER_H_

#include <stdint.h>
#include <sys/types.h>

#include "base/macros.h"
#include "util/linux/ptrace_connection.h"
#include "util/linux/ptracer.h"
#include "util/linux/thread_info.h"
#include "util/misc/address_types.h"

namespace crashpad {

//! \brief Implements a PtraceConnection over a socket.
//!
//! This class is the server half of the connection. The broker should be run
//! in a process with `ptrace` capabilities for the target process and may run
//! in a compromised context.
class PtraceBroker {
 public:
#pragma pack(push, 1)
  //! \brief A request sent to a PtraceBroker from a PtraceClient.
  struct Request {
    static constexpr uint16_t kVersion = 1;
    static constexpr size_t kMaxReadMemorySize = 1024;

    //! \brief The version number for this Request.
    uint16_t version = kVersion;

    //! \brief The type of request to execute.
    enum class Type : uint16_t {
      //! \brief `ptrace`-attach the specified thread ID. Responds with
      //!     Bool::kTrue on success, otherwise Bool::kFalse.
      kAttach,

      //! \brief Responds with Bool::kTrue if the target process is 64-bit.
      //!     Otherwise, Bool::kFalse.
      kIs64Bit,

      //! \brief Responds with a GetThreadInfoResponse containing a ThreadInfo
      //!     for the specified thread ID.
      kGetThreadInfo,

      //! \brief Reads memory from the attached process. Responds with
      //!     Bool::kTrue on success, followed by the requested buffer of data,
      //!     otherwise Boo::kFalse.
      kReadMemory,

      //! \brief Causes the broker to return from Run(), detaching all attached
      //!     threads. Does not respond.
      kExit
    } type;

    //! \brief The thread ID associated with this request. Valid for kAttach,
    //!     kGetThreadInfo, and kReadMemory.
    pid_t tid;

    //! \brief Specifies the memory region to read for a kReadMemory request.
    struct {
      //! \brief The base address of the memory region.
      VMAddress base;

      //! \brief The size of the memory region, which must be less than or
      //!     equal to kMaxReadMemorySize.
      VMSize size;
    } iov;
  };

  //! \brief A boolean status suitable for communication between processes.
  enum class Bool : char { kFalse, kTrue };

  //! \brief The response sent for a Request::Type::kGetThreadInfo.
  struct GetThreadInfoResponse {
    //! \brief Information about the specified thread. Only valid if status is
    //!     Bool::kTrue.
    ThreadInfo info;

    //! \brief Specifies the success or failure of this call.
    Bool success;
  };

#pragma pack(pop)

  //! \brief Constructs this object.
  //!
  //! \param[in] sock A socket on which to read requests from a connected
  //!     PtraceClient. Does not take ownership of the socket.
  //! \param[in] is_64_bit Whether this broker should be configured to trace a
  //!     64 bit process.
  PtraceBroker(int sock, bool is_64_bit);

  ~PtraceBroker();

  //! \brief Begin serving requests on the configured socket.
  //!
  //! This method returns when a PtraceBrokerRequest with Type::kExit is
  //! received or an error is encountered on the socket.
  //!
  //! \return `true` if Run() exited due to an exit request. Otherwise `false`.
  bool Run();

 private:
  bool ReadMemory(pid_t pid, VMAddress address, size_t size, char* buffer);

  Ptracer ptracer_;
  int sock_;

  DISALLOW_COPY_AND_ASSIGN(PtraceBroker);
};

}  // namespace crashpad

#endif  // CRASHPAD_UTIL_LINUX_PTRACE_BROKER_H_
