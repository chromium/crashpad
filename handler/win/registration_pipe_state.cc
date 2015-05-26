// Copyright 2015 The Crashpad Authors. All rights reserved.
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

#include "handler/win/registration_pipe_state.h"

#include <string.h>

#include <vector>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "util/stdlib/pointer_container.h"

namespace crashpad {

RegistrationPipeState::RegistrationPipeState(
    ScopedFileHANDLE pipe,
    RegistrationServer::Delegate* delegate)
    : request_(),
      response_(),
      completion_handler_(nullptr),
      overlapped_(),
      event_(),
      pipe_(pipe.Pass()),
      waiting_for_close_(false),
      delegate_(delegate),
      get_named_pipe_client_process_id_proc_(nullptr) {
  HMODULE kernel_dll = GetModuleHandle(L"kernel32.dll");
  if (kernel_dll) {
    get_named_pipe_client_process_id_proc_ =
        reinterpret_cast<decltype(GetNamedPipeClientProcessId)*>(
            GetProcAddress(kernel_dll, "GetNamedPipeClientProcessId"));
  }
}

RegistrationPipeState::~RegistrationPipeState() {
}

bool RegistrationPipeState::Initialize() {
  DCHECK(!event_.is_valid());
  DCHECK(pipe_.is_valid());

  event_.reset(CreateEvent(nullptr, true, false, nullptr));

  if (!event_.is_valid()) {
    PLOG(ERROR) << "CreateEvent";
  } else {
    overlapped_.hEvent = event_.get();
    if (IssueConnect())
      return true;
  }

  overlapped_.hEvent = nullptr;
  event_.reset();
  pipe_.reset();
  completion_handler_ = nullptr;

  return false;
}

void RegistrationPipeState::Stop() {
  DCHECK(pipe_.is_valid());
  if (!CancelIo(pipe_.get()))
    PLOG(FATAL) << "CancelIo";
}

bool RegistrationPipeState::OnCompletion() {
  AsyncCompletionHandler completion_handler = completion_handler_;
  completion_handler_ = nullptr;

  DWORD bytes_transferred = 0;
  BOOL success = GetOverlappedResult(pipe_.get(),
                                     &overlapped_,
                                     &bytes_transferred,
                                     false);  // Do not wait.
  if (!success) {
    // ERROR_BROKEN_PIPE is expected when we are waiting for the client to close
    // the pipe (signaling that they are done reading the response).
    if (!waiting_for_close_ || GetLastError() != ERROR_BROKEN_PIPE)
      PLOG(ERROR) << "GetOverlappedResult";
  }

  bool still_running = false;
  if (!ResetEvent(event_.get())) {
    PLOG(ERROR) << "ResetEvent";
  } else if (!completion_handler) {
    NOTREACHED();
    still_running = ResetConnection();
  } else if (!success) {
    still_running = ResetConnection();
  } else {
    still_running = (this->*completion_handler)(bytes_transferred);
  }

  if (!still_running) {
    overlapped_.hEvent = nullptr;
    event_.reset();
    pipe_.reset();
    completion_handler_ = nullptr;
  } else {
    DCHECK(completion_handler_);
  }

  return still_running;
}

bool RegistrationPipeState::OnConnectComplete(DWORD /* bytes_transferred */) {
  return IssueRead();
}

bool RegistrationPipeState::OnReadComplete(DWORD bytes_transferred) {
  if (bytes_transferred != sizeof(request_)) {
    LOG(ERROR) << "Invalid message size: " << bytes_transferred;
    return ResetConnection();
  } else {
    return HandleRequest();
  }
}

bool RegistrationPipeState::OnWriteComplete(DWORD bytes_transferred) {
  if (bytes_transferred != sizeof(response_)) {
    LOG(ERROR) << "Incomplete write operation. Bytes written: "
               << bytes_transferred;
  }

  return IssueWaitForClientClose();
}

bool RegistrationPipeState::OnWaitForClientCloseComplete(
    DWORD bytes_transferred) {
  LOG(ERROR) << "Unexpected extra data (" << bytes_transferred
             << " bytes) received from client.";
  return ResetConnection();
}

bool RegistrationPipeState::IssueConnect() {
  if (ConnectNamedPipe(pipe_.get(), &overlapped_)) {
    return OnConnectComplete(0);  // bytes_transferred (ignored)
  } else {
    DWORD result = GetLastError();
    if (result == ERROR_PIPE_CONNECTED) {
      return OnConnectComplete(0);  // bytes_transferred (ignored)
    } else if (result == ERROR_IO_PENDING) {
      completion_handler_ = &RegistrationPipeState::OnConnectComplete;
      return true;
    } else {
      PLOG(ERROR) << "ConnectNamedPipe";
      return false;
    }
  }
}

bool RegistrationPipeState::IssueRead() {
  DWORD bytes_read = 0;
  if (ReadFile(pipe_.get(),
               &request_,
               sizeof(request_),
               &bytes_read,
               &overlapped_)) {
    return OnReadComplete(bytes_read);
  } else if (GetLastError() == ERROR_IO_PENDING) {
    completion_handler_ = &RegistrationPipeState::OnReadComplete;
    return true;
  } else {
    PLOG(ERROR) << "ReadFile";
    return ResetConnection();
  }
}

bool RegistrationPipeState::IssueWrite() {
  DWORD bytes_written = 0;
  if (WriteFile(pipe_.get(),
                &response_,
                sizeof(response_),
                &bytes_written,
                &overlapped_)) {
    return OnWriteComplete(bytes_written);
  } else if (GetLastError() == ERROR_IO_PENDING) {
    completion_handler_ = &RegistrationPipeState::OnWriteComplete;
    return true;
  } else {
    PLOG(ERROR) << "WriteFile";
    return ResetConnection();
  }
}

bool RegistrationPipeState::IssueWaitForClientClose() {
  // If we invoke DisconnectNamedPipe before the client has read the response
  // the response will never be delivered. Therefore we issue an extra ReadFile
  // operation after writing the response. No data is expected - the operation
  // will be 'completed' when the client closes the pipe.
  waiting_for_close_ = true;
  DWORD bytes_read = 0;
  if (ReadFile(pipe_.get(),
               &request_,
               sizeof(request_),
               &bytes_read,
               &overlapped_)) {
    return OnWaitForClientCloseComplete(bytes_read);
  } else if (GetLastError() == ERROR_IO_PENDING) {
    completion_handler_ = &RegistrationPipeState::OnWaitForClientCloseComplete;
    return true;
  } else {
    PLOG(ERROR) << "ReadFile";
    return ResetConnection();
  }
}

bool RegistrationPipeState::HandleRequest() {
  if (get_named_pipe_client_process_id_proc_) {
    // On Vista+ we can verify that the client is who they claim to be, thus
    // preventing arbitrary processes from having us duplicate handles into
    // other processes.
    DWORD real_client_process_id = 0;
    if (!get_named_pipe_client_process_id_proc_(pipe_.get(),
                                                &real_client_process_id)) {
      PLOG(ERROR) << "GetNamedPipeClientProcessId";
    } else if (real_client_process_id != request_.client_process_id) {
      LOG(ERROR) << "Client process ID from request ("
                 << request_.client_process_id
                 << ") does not match pipe client process ID ("
                 << real_client_process_id << ").";
      return ResetConnection();
    }
  }

  ScopedKernelHANDLE client_process(
      OpenProcess(PROCESS_ALL_ACCESS, false, request_.client_process_id));
  if (!client_process.is_valid()) {
    if (ImpersonateNamedPipeClient(pipe_.get())) {
      client_process.reset(
          OpenProcess(PROCESS_ALL_ACCESS, false, request_.client_process_id));
      RevertToSelf();
    }
  }

  if (!client_process.is_valid()) {
    LOG(ERROR) << "Failed to open client process.";
    return ResetConnection();
  }

  memset(&response_, 0, sizeof(response_));

  HANDLE request_report_event = nullptr;
  HANDLE report_complete_event = nullptr;

  if (!delegate_->RegisterClient(client_process.Pass(),
                                 request_.crashpad_info_address,
                                 &request_report_event,
                                 &report_complete_event)) {
    return ResetConnection();
  }

  // A handle has at most 32 significant bits, though its type is void*. Thus we
  // truncate it here. An interesting exception is INVALID_HANDLE_VALUE, which
  // is '-1'. It is still safe to truncate it from 0xFFFFFFFFFFFFFFFF to
  // 0xFFFFFFFF, but a 64-bit client receiving that value must correctly sign
  // extend it.
  response_.request_report_event =
      reinterpret_cast<uint32_t>(request_report_event);
  response_.report_complete_event =
      reinterpret_cast<uint32_t>(report_complete_event);
  return IssueWrite();
}

bool RegistrationPipeState::ResetConnection() {
  memset(&request_, 0, sizeof(request_));
  waiting_for_close_ = false;

  if (!DisconnectNamedPipe(pipe_.get())) {
    PLOG(ERROR) << "DisconnectNamedPipe";
    return false;
  } else {
    return IssueConnect();
  }
}

}  // namespace crashpad
