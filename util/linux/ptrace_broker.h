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

#include <errno.h>
#include <stdint.h>
#include <sys/types.h>

#include "base/macros.h"
#include "util/linux/exception_handler_protocol.h"
#include "util/linux/ptrace_connection.h"
#include "util/linux/ptracer.h"
#include "util/linux/scoped_ptrace_attach.h"
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

    //! \brief The version number for this Request.
    uint16_t version = kVersion;

    //! \brief The type of request to serve.
    enum Type : uint16_t {
      //! \brief `ptrace`-attach the specified thread ID. Responds with
      //!     kBoolTrue on success, otherwise kBoolFalse, followed by an Errno.
      kTypeAttach,

      //! \brief Responds with kBoolTrue if the target process is 64-bit.
      //!     Otherwise, kBoolFalse.
      kTypeIs64Bit,

      //! \brief Responds with a GetThreadInfoResponse containing a ThreadInfo
      //!     for the specified thread ID. If an error occurs,
      //!     GetThreadInfoResponse::success is set to kBoolFalse and is
      //!     followed by an Errno.
      kTypeGetThreadInfo,

      //! \brief Initialize a memory reader using the specified file path.
      kTypeSetMemoryFile,

      //! \brief Reads memory from the attached process. The data is returned in
      //!     a series of messages. Each message begins with a VMSize indicating
      //!     the number of bytes being returned in this message, followed by
      //!     the requested bytes. The broker continues to send messages until
      //!     either all of the requested memory has been sent or an error
      //!     occurs, in which case it sends a message containing a VMSize equal
      //!     to zero, followed by an Errno.
      kTypeReadMemory,

      //! \brief Read a file's contents. The data is returned in a series of
      //!     messages. Each message begins with ReadResult indicating the
      //!     number of bytes read, the end of file, or an error condition. On
      //!     success, the bytes read follow the ReadResult.
      kTypeReadFile,

      //! \brief Causes the broker to return from Run(), detaching all attached
      //!     threads. Does not respond.
      kTypeExit
    } type;

    //! \brief The thread ID associated with this request. Valid for kTypeAttach,
    //!     kTypeGetThreadInfo, and kTypeReadMemory.
    pid_t tid;

    union {
      //! \brief Specifies the memory region to read for a kTypeReadMemory
      //! request.
      struct {
        //! \brief The base address of the memory region.
        VMAddress base;

        //! \brief The size of the memory region.
        VMSize size;
      } iov;

      // \brief Specifies the file path to read for a kTypeReadFile request.
      struct {
        //! \brief The number of bytes in #path, including the `NUL`-terminator.
        VMSize path_length;

        //! \brief The file path to read.
        char path[];
      } path;
    };
  };

  //! \brief The result of a read operation.
  enum ReadResult : int32_t {
    //! \brief Access to the path is denied.
    kReadResultAccessDenied = -3,

    //! \brief The path name is too long.
    kReadResultPathTooLong = -2,

    //! \brief An Errno follows.
    kReadResultErrno = -1,

    //! \brief The end-of-file was reached.
    kReadResultEOF = 0,
  };

  //! \brief The response sent for a Request with type kTypeGetThreadInfo.
  struct GetThreadInfoResponse {
    //! \brief Information about the specified thread. Only valid if #success
    //!     is kBoolTrue.
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
  //!     64-bit process.
  PtraceBroker(int sock, bool is_64_bit);

  ~PtraceBroker();

  //! \brief Restricts the broker to serving the contents of files under \a
  //!     root.
  //!
  //! If this method is not called, the broker defaults to only serving files
  //! under "/proc/".
  //!
  //! \param[in] root A NUL-terminated c-string containing the path to the new
  //!     root. \a root must not be `nullptr` and the caller should ensure that
  //!     \a root reamins valid for the lifetime of the broker.
  void SetFileRoot(const char* root);

  //! \brief Begin serving requests on the configured socket.
  //!
  //! This method returns when a PtraceBrokerRequest with type kTypeExit is
  //! received or an error is encountered on the socket.
  //!
  //! This method calls `sbrk`, which may break other memory management tools,
  //! such as `malloc`.
  //!
  //! \return 0 if Run() exited due to an exit request. Otherwise an error code.
  int Run();

 private:
  int RunImpl();
  int SendError(Errno err);
  int SendReadError(ReadResult rv, Errno err);
  int SendFileContents(char* path);
  int SendMemory(pid_t pid, VMAddress address, VMSize size);
  bool AllocateAttachments();
  void ReleaseAttachments();
  int ReceiveFilePath(VMSize path_length,
                      char* buffer,
                      size_t buffer_size,
                      bool* valid_path);

  Ptracer ptracer_;
  const char* file_root_;
  ScopedPtraceAttach* attachments_;
  size_t attach_count_;
  size_t attach_capacity_;
  ScopedFileHandle memory_file_;
  int sock_;

  DISALLOW_COPY_AND_ASSIGN(PtraceBroker);
};

}  // namespace crashpad

#endif  // CRASHPAD_UTIL_LINUX_PTRACE_BROKER_H_
