// Copyright 2019 The Crashpad Authors. All rights reserved.
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

#ifndef CRASHPAD_UTIL_LINUX_SOCKET_H_
#define CRASHPAD_UTIL_LINUX_SOCKET_H_

#include <sys/socket.h>
#include <sys/types.h>

#include "util/file/file_io.h"

namespace crashpad {

//! \brief Wraps `socketpair()` to create `AF_UNIX` family socket pairs.
//!
//! \param[out] s1 One end of the connected pair.
//! \param[out] s2 The other end of the connected pair.
//! \return `true` on success. Otherwise, `false` with a message logged.
bool UnixSocketpair(ScopedFileHandle* s1, ScopedFileHandle* s2);

//! \brief Wraps `sendmsg()` to send a message with credentials.
//!
//! This function is intended for use with `AF_UNIX` family sockets and
//! and includes an `SCM_CREDENTIALS` control message with this process'
//! credentials.
//!
//! \param[in] fd The file descriptor to write the message to.
//! \param[in] buf The buffer containing the message.
//! \param[in] buf_size The size of the message.
//! \return 0 on success or an error code on failure.
int SendMsg(int fd, const void* buf, size_t buf_size);

//! \brief Wraps `recvmsg()` to receive a message with credentials.
//!
//! This function is intended to be used with `AF_UNIX` family sockets. Any file
//! descriptors included in the message (via `SCM_RIGHTS`) are closed,
//! regardless of whether this function succeeds. The socket must have
//! `SO_PASSCRED` set and the sender must include credentials in the message or
//! this function returns `false`.
//!
//! \param[in] fd The file descriptor to receive the message on.
//! \param[out] buf The buffer to fill with the message.
//! \param[in] buf_size The size of the message.
//! \param[out] creds The credentials of the sender.
//! \return `true` on success. Otherwise, `false`, with a message logged.
bool RecvMsg(int fd, void* buf, size_t buf_size, ucred* creds);

}  // namespace crashpad

#endif  // CRASHPAD_UTIL_LINUX_SOCKET_H_
