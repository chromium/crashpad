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

#include "util/win/session_end_watcher.h"

#include "base/logging.h"

extern "C" {
extern IMAGE_DOS_HEADER __ImageBase;
}  // extern "C"

namespace crashpad {

namespace {

// Calls a lambda when going out of scope. This is “defer” for the poor.
template <typename T>
class ScopedCallLambda {
 public:
  ScopedCallLambda(const T& lambda) : lambda_(lambda) {}
  ~ScopedCallLambda() { lambda_(); }

 private:
  const T& lambda_;

  DISALLOW_COPY_AND_ASSIGN(ScopedCallLambda);
};

// GetWindowLongPtr()’s return value doesn’t unambiguously indicate whether it
// was successful, because 0 could either represent successful retrieval of the
// value 0, or failure. This wrapper is more convenient to use.
bool GetWindowLongPtrAndSuccess(HWND window, int index, LONG_PTR* value) {
  SetLastError(ERROR_SUCCESS);
  *value = GetWindowLongPtr(window, index);
  return *value || GetLastError() == ERROR_SUCCESS;
}

// SetWindowLongPtr() has the same problem as GetWindowLongPtr(). Use this
// wrapper instead.
bool SetWindowLongPtrAndGetSuccess(HWND window, int index, LONG_PTR value) {
  SetLastError(ERROR_SUCCESS);
  LONG_PTR previous = SetWindowLongPtr(window, index, value);
  return previous || GetLastError() == ERROR_SUCCESS;
}

}  // namespace

SessionEndWatcher::SessionEndWatcher()
    : thread_(CreateThread(nullptr, 0, ThreadMain, this, 0, nullptr)),
      window_() {
  PLOG_IF(ERROR, !thread_.get()) << "CreateThread";
}

SessionEndWatcher::~SessionEndWatcher() {
  if (thread_.get()) {
    DWORD result = WaitForSingleObject(thread_.get(), INFINITE);
    PLOG_IF(ERROR, result != WAIT_OBJECT_0) << "WaitForSingleObject";
  }

  DCHECK(!window_);
}

// static
DWORD WINAPI SessionEndWatcher::ThreadMain(void* argument) {
  const DWORD kSuccess = 0;
  const DWORD kFailure = 1;

  SessionEndWatcher* self = reinterpret_cast<SessionEndWatcher*>(argument);

  WNDCLASS wndclass = {};
  wndclass.lpfnWndProc = WindowProc;
  wndclass.hInstance = reinterpret_cast<HMODULE>(&__ImageBase);
  wndclass.lpszClassName = L"crashpad_SessionEndWatcher";
  ATOM atom = RegisterClass(&wndclass);
  if (!atom) {
    PLOG(ERROR) << "RegisterClass";
    return kFailure;
  }

  self->window_ =
      CreateWindow(MAKEINTATOM(atom), nullptr, 0, 0, 0, 0, 0, 0, 0, 0, self);
  if (!self->window_) {
    PLOG(ERROR) << "CreateWindow";
    return kFailure;
  }

  self->SessionEndWatcherEvent(Event::kStartedWatching);

  MSG message;
  BOOL rv = 0;
  while (self->window_ &&
         (rv = GetMessage(&message, self->window_, 0, 0)) > 0) {
    TranslateMessage(&message);
    DispatchMessage(&message);
  }

  {
    // Arrange to send the kStoppedWatching notification when exiting this
    // scope. That ensures that SessionEndWatcherEvent() doesn’t mess with
    // GetLastError() before one of the PLOG(ERROR)s that follow has a chance to
    // figure out the reason for a failure.
    auto lambda = [self]() {
      self->SessionEndWatcherEvent(Event::kStoppedWatching);
    };
    ScopedCallLambda<decltype(lambda)> call_stopped_watching(lambda);

    if (self->window_) {
      if (rv == -1) {
        PLOG(ERROR) << "GetMessage";
        return kFailure;
      }

      // If the message loop terminated because a WM_DESTROY message was
      // processed, DestroyWindow() will have already been called and
      // self->window_ will be nullptr. But if the message loop terminated
      // because it processed a WM_QUIT message, the window still exists and
      // needs to be cleaned up.
      rv = DestroyWindow(self->window_);
      if (!rv) {
        PLOG(ERROR) << "DestroyWindow";
        return kFailure;
      }
      self->window_ = 0;
    }
  }

  rv = UnregisterClass(MAKEINTATOM(atom), 0);
  if (!rv) {
    PLOG(ERROR) << "UnregisterClass";
    return kFailure;
  }

  return kSuccess;
}

// static
LRESULT CALLBACK SessionEndWatcher::WindowProc(HWND window,
                                               UINT message,
                                               WPARAM w_param,
                                               LPARAM l_param) {
  // Figure out which object this is. A pointer to it is stuffed into the last
  // parameter of CreateWindow(), which shows up as CREATESTRUCT::lpCreateParams
  // in a WM_CREATE message. That should be processed before any of the other
  // messages of interest to this function. Once the object is known, save a
  // pointer to it in the GWLP_USERDATA slot for later retrieval when processing
  // other messages.
  SessionEndWatcher* self;
  if (!GetWindowLongPtrAndSuccess(
          window, GWLP_USERDATA, reinterpret_cast<LONG_PTR*>(&self))) {
    PLOG(ERROR) << "GetWindowLongPtr";
  }
  if (!self && message == WM_CREATE) {
    CREATESTRUCT* create = reinterpret_cast<CREATESTRUCT*>(l_param);
    self = reinterpret_cast<SessionEndWatcher*>(create->lpCreateParams);
    if (!SetWindowLongPtrAndGetSuccess(
            window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self))) {
      PLOG(ERROR) << "SetWindowLongPtr";
    }
  }

  if (self) {
    if (message == WM_ENDSESSION) {
      // If w_param is false, this WM_ENDSESSION cancels a previous
      // WM_QUERYENDSESSION.
      if (w_param) {
        self->SessionEndWatcherEvent(Event::kSessionEnding);

        // If the session is ending, post a close message which will kick off
        // window destruction and cause the message loop thread to terminate.
        if (!PostMessage(self->window_, WM_CLOSE, 0, 0)) {
          PLOG(ERROR) << "PostMessage";
        }
      }
    } else if (message == WM_DESTROY) {
      // The window is being destroyed. Clear GWLP_USERDATA so that |self| won’t
      // be found during a subsequent call into this function for this window.
      // Clear self->window_ too, because it refers to an object that soon won’t
      // exist. That signals the message loop to stop processing messages.
      if (!SetWindowLongPtrAndGetSuccess(window, GWLP_USERDATA, 0)) {
        PLOG(ERROR) << "SetWindowLongPtr";
      }
      self->window_ = 0;
    }
  }

  // If |message| was WM_CLOSE, DefWindowProc() will call DestroyWindow(), and
  // this function will be called again with WM_DESTROY.
  return DefWindowProc(window, message, w_param, l_param);
}

}  // namespace crashpad
