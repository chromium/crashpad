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

#include "handler/win/registration_server.h"

#include <vector>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "handler/win/registration_pipe_state.h"
#include "util/stdlib/pointer_container.h"

namespace crashpad {

RegistrationServer::RegistrationServer() : stop_event_() {
  stop_event_.reset(CreateEvent(nullptr, false, false, nullptr));
  DPCHECK(stop_event_.is_valid());
}

RegistrationServer::~RegistrationServer() {
}

bool RegistrationServer::Run(const base::string16& pipe_name,
                             Delegate* delegate) {
  if (!stop_event_.is_valid()) {
    LOG(ERROR) << "Failed to create stop_event_.";
    return false;
  }

  PointerVector<RegistrationPipeState> pipes;
  std::vector<HANDLE> handles;

  const int kNumPipes = 3;

  // Create the named pipes.
  for (int i = 0; i < kNumPipes; ++i) {
    DWORD open_mode = PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED;
    if (pipes.size() == 0)
      open_mode |= FILE_FLAG_FIRST_PIPE_INSTANCE;
    ScopedFileHANDLE pipe(
        CreateNamedPipe(pipe_name.c_str(),
                        open_mode,
                        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
                        kNumPipes,
                        512,  // nOutBufferSize
                        512,  // nInBufferSize
                        20,  // nDefaultTimeOut
                        nullptr));  // lpSecurityAttributes
    if (pipe.is_valid()) {
      scoped_ptr<RegistrationPipeState> pipe_state(
          new RegistrationPipeState(pipe.Pass(), delegate));
      if (pipe_state->Initialize()) {
        pipes.push_back(pipe_state.release());
        handles.push_back(pipes.back()->completion_event());
      }
    } else {
      PLOG(ERROR) << "CreateNamedPipe";
    }
  }

  if (pipes.size() == 0) {
    LOG(ERROR) << "Failed to initialize any pipes.";
    return false;
  }

  delegate->OnStarted();

  // Add stop_event_ to the list of events we will observe.
  handles.push_back(stop_event_.get());

  bool stopped = false;

  // Run the main loop, dispatching completion event signals to the pipe
  // instances.
  while (true) {
    DWORD wait_result = WaitForMultipleObjects(
        static_cast<DWORD>(handles.size()), handles.data(), false, INFINITE);
    if (wait_result >= WAIT_OBJECT_0 &&
        wait_result < WAIT_OBJECT_0 + pipes.size()) {
      int index = wait_result - WAIT_OBJECT_0;
      // Handle a completion event.
      if (!pipes[index]->OnCompletion()) {
        pipes.erase(pipes.begin() + index);
        handles.erase(handles.begin() + index);
      }
      if (pipes.size())
        continue;
      // Exit due to all pipes having failed.
    } else if (wait_result == WAIT_OBJECT_0 + pipes.size()) {
      // Exit due to stop_event_.
      stopped = true;
    } else if (wait_result == WAIT_FAILED) {
      // Exit due to error.
      PLOG(ERROR) << "WaitForMultipleObjects";
    } else {
      // Exit due to unexpected return code.
      NOTREACHED();
    }
    break;
  }

  // Remove |stop_event_| from the wait list.
  handles.pop_back();

  // Cancel any ongoing asynchronous operations.
  for (auto& pipe : pipes) {
    pipe->Stop();
  }

  // Wait until all of the pipe instances are ready to be destroyed.
  DWORD wait_result = WaitForMultipleObjects(
      static_cast<DWORD>(handles.size()), handles.data(), true, INFINITE);
  PCHECK(wait_result != WAIT_FAILED);
  DCHECK_GE(wait_result, WAIT_OBJECT_0);
  DCHECK_LT(wait_result, WAIT_OBJECT_0 + handles.size());

  return stopped;
}

void RegistrationServer::Stop() {
  if (!SetEvent(stop_event_.get()))
    PLOG(FATAL) << "SetEvent";
}

}  // namespace crashpad
