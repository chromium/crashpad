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

#include <windows.h>

#include "base/basictypes.h"
#include "base/strings/string16.h"
#include "client/registration_protocol_win.h"
#include "gtest/gtest.h"
#include "handler/win/registration_server.h"
#include "util/stdlib/pointer_container.h"
#include "util/win/address_types.h"
#include "util/win/scoped_handle.h"

namespace crashpad {
namespace test {

class RegistrationTestBase : public testing::Test {
 public:
  // Simulates a registrar to collect requests from and feed responses to the
  // RegistrationServer.
  class MockDelegate : public RegistrationServer::Delegate {
   public:
    // Records a single simulated client registration.
    struct Entry {
      Entry(ScopedKernelHANDLE client_process,
            WinVMAddress crashpad_info_address,
            uint32_t fake_request_dump_event,
            uint32_t fake_dump_complete_event);

      ScopedKernelHANDLE client_process;
      WinVMAddress crashpad_info_address;
      uint32_t fake_request_dump_event;
      uint32_t fake_dump_complete_event;
    };

    MockDelegate();
    ~MockDelegate() override;

    // Blocks until RegistrationServer::Delegate::OnStarted is invoked.
    void WaitForStart();

    // RegistrationServer::Delegate:
    void OnStarted() override;

    bool RegisterClient(ScopedKernelHANDLE client_process,
                        WinVMAddress crashpad_info_address,
                        HANDLE* request_dump_event,
                        HANDLE* dump_complete_event) override;

    // Provides access to the registered process data.
    const std::vector<Entry*> registered_processes() {
      return registered_processes_;
    }

    // If true, causes RegisterClient to simulate registration failure.
    void set_fail_mode(bool fail) { fail_ = fail; }

   private:
    ScopedKernelHANDLE started_event_;
    PointerVector<Entry> registered_processes_;
    uint32_t next_fake_handle_;
    bool fail_;

    DISALLOW_COPY_AND_ASSIGN(MockDelegate);
  };

  RegistrationTestBase();
  ~RegistrationTestBase() override;

  MockDelegate& delegate() { return delegate_; }
  base::string16 pipe_name() { return pipe_name_; }

  // Returns a pipe handle connected to the RegistrationServer.
  ScopedFileHANDLE Connect();

  // Sends the provided request and receives a response via the provided pipe.
  bool SendRequest(ScopedFileHANDLE pipe,
                   const void* request_buffer,
                   size_t request_size,
                   RegistrationResponse* response);

  bool WriteRequest(HANDLE pipe,
                    const void* request_buffer,
                    size_t request_size);

  bool ReadResponse(HANDLE pipe, RegistrationResponse* response);

  // Verifies that the request and response match what was received and sent by
  // the MockDelegate.
  void VerifyRegistration(const MockDelegate::Entry& registered_process,
                          const RegistrationRequest& request,
                          const RegistrationResponse& response);

 private:
  base::string16 pipe_name_;
  MockDelegate delegate_;

  DISALLOW_COPY_AND_ASSIGN(RegistrationTestBase);
};

}  // namespace test
}  // namespace crashpad
