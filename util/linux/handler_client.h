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

#ifndef CRASHPAD_UTIL_LINUX_HANDLER_CLIENT_H_
#define CRASHPAD_UTIL_LINUX_HANDLER_CLIENT_H_

#include <sys/types.h>

#include "util/linux/handler_protocol.h"

namespace crashpad {

//! A client for an ExceptionHandlerServer
class HandlerClient {
 public:
  HandlerClient(int sock);
  ~HandlerClient();

  //! \brief Signal a crash dump request on a socket.
  //!
  //! This method sends a request to the server but does not block for the
  //! server to complete the request. The caller can call WaitForCrashDump to
  //! get the success status of this operation.
  //!
  //! \return `true` on success. `false` on failure with a message logged.
  int RequestCrashDump(const ClientInformation& info);

  int WaitForCrashDumpComplete();

  int SetPtracer(pid_t pid);

 private:
  int server_sock_;
  pid_t ptracer_;
  bool can_set_ptracer_;
};

}  // namespace crashpad

#endif  // CRASHPAD_UTIL_LINUX_HANDLER_CLIENT_H_
