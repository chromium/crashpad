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

#include "handler/win/registration_test_base.h"

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"

namespace crashpad {
namespace test {

RegistrationTestBase::MockDelegate::Entry::Entry(
    ScopedKernelHANDLE client_process,
    WinVMAddress crashpad_info_address,
    uint32_t fake_request_dump_event,
    uint32_t fake_dump_complete_event)
    : client_process(client_process.Pass()),
      crashpad_info_address(crashpad_info_address),
      fake_request_dump_event(fake_request_dump_event),
      fake_dump_complete_event(fake_dump_complete_event) {
}
RegistrationTestBase::MockDelegate::MockDelegate()
    : started_event_(CreateEvent(nullptr, true, false, nullptr)),
      registered_processes_(),
      next_fake_handle_(1),
      fail_(false) {
  EXPECT_TRUE(started_event_.is_valid());
}

RegistrationTestBase::MockDelegate::~MockDelegate() {
}

void RegistrationTestBase::MockDelegate::WaitForStart() {
  DWORD wait_result = WaitForSingleObject(started_event_.get(), INFINITE);
  if (wait_result == WAIT_FAILED)
    PLOG(ERROR);
  ASSERT_EQ(wait_result, WAIT_OBJECT_0);
}

void RegistrationTestBase::MockDelegate::OnStarted() {
  EXPECT_EQ(WAIT_TIMEOUT, WaitForSingleObject(started_event_.get(), 0));
  SetEvent(started_event_.get());
}

bool RegistrationTestBase::MockDelegate::RegisterClient(
    ScopedKernelHANDLE client_process,
    WinVMAddress crashpad_info_address,
    HANDLE* request_dump_event,
    HANDLE* dump_complete_event) {
  if (fail_)
    return false;

  if (!request_dump_event || !dump_complete_event) {
    ADD_FAILURE() << "NULL 'out' parameter.";
    return false;
  }
  *request_dump_event = reinterpret_cast<HANDLE>(next_fake_handle_++);
  *dump_complete_event = reinterpret_cast<HANDLE>(next_fake_handle_++);

  registered_processes_.push_back(
      new Entry(client_process.Pass(),
                crashpad_info_address,
                reinterpret_cast<uint32_t>(*request_dump_event),
                reinterpret_cast<uint32_t>(*dump_complete_event)));
  return true;
}

RegistrationTestBase::RegistrationTestBase()
    : pipe_name_(
          L"\\\\.\\pipe\\registration_server_test_pipe_" +
          base::UTF8ToUTF16(base::StringPrintf("%08x", GetCurrentProcessId()))),
      delegate_() {
}

RegistrationTestBase::~RegistrationTestBase() {
}

// Returns a pipe handle connected to the RegistrationServer.
ScopedFileHANDLE RegistrationTestBase::Connect() {
  ScopedFileHANDLE pipe;
  const int kMaxRetries = 5;
  for (int retries = 0; !pipe.is_valid() && retries < kMaxRetries; ++retries) {
    if (!WaitNamedPipe(pipe_name_.c_str(), NMPWAIT_WAIT_FOREVER))
      break;
    pipe.reset(CreateFile(pipe_name_.c_str(),
                          GENERIC_READ | GENERIC_WRITE,
                          0,
                          NULL,
                          OPEN_EXISTING,
                          SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION,
                          NULL));
    if (pipe.is_valid()) {
      DWORD mode = PIPE_READMODE_MESSAGE;
      if (!SetNamedPipeHandleState(pipe.get(),
                                   &mode,
                                   nullptr,  // don't set maximum bytes
                                   nullptr)) {  // don't set maximum time
        pipe.reset();
      }
    }
  }
  EXPECT_TRUE(pipe.is_valid());
  return pipe.Pass();
}

// Sends the provided request and receives a response via the provided pipe.
bool RegistrationTestBase::SendRequest(ScopedFileHANDLE pipe,
                                       const void* request_buffer,
                                       size_t request_size,
                                       RegistrationResponse* response) {
  return WriteRequest(pipe.get(), request_buffer, request_size) &&
         ReadResponse(pipe.get(), response);
}

bool RegistrationTestBase::WriteRequest(HANDLE pipe,
                                        const void* request_buffer,
                                        size_t request_size) {
  DWORD byte_count = 0;
  if (!WriteFile(pipe,
                 request_buffer,
                 static_cast<DWORD>(request_size),
                 &byte_count,
                 nullptr)) {
    return false;
  }

  return byte_count == request_size;
}

bool RegistrationTestBase::ReadResponse(HANDLE pipe,
                                        RegistrationResponse* response) {
  DWORD byte_count = 0;
  if (!ReadFile(pipe, response, sizeof(*response), &byte_count, nullptr))
    return false;
  return byte_count == sizeof(*response);
}

// Verifies that the request and response match what was received and sent by
// the MockDelegate.
void RegistrationTestBase::VerifyRegistration(
    const MockDelegate::Entry& registered_process,
    const RegistrationRequest& request,
    const RegistrationResponse& response) {
  EXPECT_EQ(request.crashpad_info_address,
            registered_process.crashpad_info_address);
  EXPECT_EQ(registered_process.fake_request_dump_event,
            response.request_report_event);
  EXPECT_EQ(registered_process.fake_dump_complete_event,
            response.report_complete_event);
  EXPECT_EQ(request.client_process_id,
            GetProcessId(registered_process.client_process.get()));
}

}  // namespace test
}  // namespace crashpad
