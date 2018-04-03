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

#include "util/linux/ptrace_broker.h"

#include <fcntl.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>

#include <algorithm>

#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "util/file/file_io.h"

namespace crashpad {

PtraceBroker::PtraceBroker(int sock, bool is_64_bit)
    : ptracer_(is_64_bit, /* can_log= */ false),
      file_root_("/proc/"),
      attachments_(nullptr),
      attach_count_(0),
      attach_capacity_(0),
      sock_(sock) {
  AllocateAttachments();
}

PtraceBroker::~PtraceBroker() = default;

void PtraceBroker::SetFileRoot(const char* new_root) {
  DCHECK_EQ(new_root[strlen(new_root) - 1], '/');
  file_root_ = new_root;
}

int PtraceBroker::Run() {
  int result = RunImpl();
  ReleaseAttachments();
  return result;
}

bool PtraceBroker::AllocateAttachments() {
  constexpr size_t page_size = 4096;
  constexpr size_t alloc_size =
      (sizeof(ScopedPtraceAttach) + page_size - 1) & ~(page_size - 1);
  void* alloc = sbrk(alloc_size);
  if (reinterpret_cast<intptr_t>(alloc) == -1) {
    return false;
  }

  if (attachments_ == nullptr) {
    attachments_ = reinterpret_cast<ScopedPtraceAttach*>(alloc);
  }

  attach_capacity_ += alloc_size / sizeof(ScopedPtraceAttach);
  return true;
}

void PtraceBroker::ReleaseAttachments() {
  for (size_t index = 0; index < attach_count_; ++index) {
    attachments_[index].Reset();
  }
}

int PtraceBroker::RunImpl() {
  while (true) {
    Request request = {};
    if (!ReadFileExactly(sock_, &request, sizeof(request))) {
      return errno;
    }

    if (request.version != Request::kVersion) {
      return EINVAL;
    }

    switch (request.type) {
      case Request::kTypeAttach: {
        ScopedPtraceAttach* attach;
        ScopedPtraceAttach stack_attach;
        bool attach_on_stack = false;

        if (attach_capacity_ > attach_count_ || AllocateAttachments()) {
          attach = new (&attachments_[attach_count_]) ScopedPtraceAttach;
        } else {
          attach = &stack_attach;
          attach_on_stack = true;
        }

        Bool status = kBoolFalse;
        if (attach->ResetAttach(request.tid)) {
          status = kBoolTrue;
          if (!attach_on_stack) {
            ++attach_count_;
          }
        }

        if (!WriteFile(sock_, &status, sizeof(status))) {
          return errno;
        }

        if (status == kBoolFalse) {
          Errno error = errno;
          if (!WriteFile(sock_, &error, sizeof(error))) {
            return errno;
          }
        }

        if (attach_on_stack && status == kBoolTrue) {
          return RunImpl();
        }
        continue;
      }

      case Request::kTypeIs64Bit: {
        Bool is_64_bit = ptracer_.Is64Bit() ? kBoolTrue : kBoolFalse;
        if (!WriteFile(sock_, &is_64_bit, sizeof(is_64_bit))) {
          return errno;
        }
        continue;
      }

      case Request::kTypeGetThreadInfo: {
        GetThreadInfoResponse response;
        response.success = ptracer_.GetThreadInfo(request.tid, &response.info)
                               ? kBoolTrue
                               : kBoolFalse;

        if (!WriteFile(sock_, &response, sizeof(response))) {
          return errno;
        }

        if (response.success == kBoolFalse) {
          Errno error = errno;
          if (!WriteFile(sock_, &error, sizeof(error))) {
            return errno;
          }
        }
        continue;
      }

      case Request::kTypeReadFile: {
        ScopedFileHandle handle;
        int result = ReceiveAndOpenFilePath(request.path.path_length, &handle);
        if (result != 0) {
          return result;
        }

        if (!handle.is_valid()) {
          continue;
        }

        result = SendFileContents(handle.get());
        if (result != 0) {
          return result;
        }
        continue;
      }

      case Request::kTypeReadMemory: {
        int result =
            SendMemory(request.tid, request.iov.base, request.iov.size);
        if (result != 0) {
          return result;
        }
        continue;
      }

      case Request::kTypeExit:
        return 0;
    }

    DCHECK(false);
    return EINVAL;
  }
}

int PtraceBroker::SendError(Errno err) {
  return WriteFile(sock_, &err, sizeof(err)) ? 0 : errno;
}

int PtraceBroker::SendReadError(Errno err) {
  int32_t rv = -1;
  if (!WriteFile(sock_, &rv, sizeof(rv))) {
    return errno;
  }
  return SendError(err);
}

int PtraceBroker::SendOpenResult(OpenResult result) {
  return WriteFile(sock_, &result, sizeof(result)) ? 0 : errno;
}

int PtraceBroker::SendFileContents(FileHandle handle) {
  char buffer[4096];
  int32_t rv;
  do {
    rv = ReadFile(handle, buffer, sizeof(buffer));

    if (rv < 0) {
      return SendReadError(errno);
    }

    if (!WriteFile(sock_, &rv, sizeof(rv))) {
      return errno;
    }

    if (rv > 0) {
      if (!WriteFile(sock_, buffer, static_cast<size_t>(rv))) {
        return errno;
      }
    }
  } while (rv > 0);

  return 0;
}

int PtraceBroker::SendMemory(pid_t pid, VMAddress address, VMSize size) {
  char buffer[4096];
  while (size > 0) {
    VMSize bytes_read = std::min(size, VMSize{sizeof(buffer)});

    if (!ptracer_.ReadMemory(pid, address, bytes_read, buffer)) {
      bytes_read = 0;
      Errno error = errno;
      if (!WriteFile(sock_, &bytes_read, sizeof(bytes_read)) ||
          !WriteFile(sock_, &error, sizeof(error))) {
        return errno;
      }
      return 0;
    }

    if (!WriteFile(sock_, &bytes_read, sizeof(bytes_read))) {
      return errno;
    }

    if (!WriteFile(sock_, buffer, bytes_read)) {
      return errno;
    }

    size -= bytes_read;
    address += bytes_read;
  }
  return 0;
}

int PtraceBroker::ReceiveAndOpenFilePath(VMSize path_length,
                                         ScopedFileHandle* handle) {
  char path[std::max(4096, PATH_MAX)];

  if (path_length >= sizeof(path)) {
    return SendOpenResult(kOpenResultTooLong);
  }

  if (!ReadFileExactly(sock_, path, path_length)) {
    return errno;
  }
  path[path_length] = '\0';

  if (strncmp(path, file_root_, strlen(file_root_)) != 0) {
    return SendOpenResult(kOpenResultAccessDenied);
  }

  ScopedFileHandle local_handle(
      HANDLE_EINTR(open(path, O_RDONLY | O_CLOEXEC | O_NOCTTY)));
  if (!local_handle.is_valid()) {
    return SendOpenResult(static_cast<OpenResult>(errno));
  }

  handle->reset(local_handle.release());
  return SendOpenResult(kOpenResultSuccess);
}

}  // namespace crashpad
