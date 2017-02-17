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

#ifndef CRASHPAD_UTIL_WIN_SESSION_END_WATCHER_H_
#define CRASHPAD_UTIL_WIN_SESSION_END_WATCHER_H_

#include <windows.h>

#include "base/macros.h"
#include "util/win/scoped_handle.h"

namespace crashpad {

//! \brief Creates a hidden window and waits for a `WM_ENDSESSION` or `WM_CLOSE`
//!     message, indicating that the session is ending and the application
//!     should terminate.
//!
//! A dedicated thread will be created to run the `GetMessage()`-based message
//! loop required to monitor for this message.
//!
//! Users should subclass this class and receive notifications by implementing
//! the SessionEndWatcherEvent() method.
class SessionEndWatcher {
 public:
  SessionEndWatcher();

  //! \note The destructor waits for the thread that runs the message loop to
  //!     terminate.
  virtual ~SessionEndWatcher();

 protected:
  //! \brief The type of event being notified.
  enum class Event {
    //! \brief A window is being monitored for messages.
    //!
    //! Because the message loop runs on a different thread than the object was
    //! created on, this event allows for synchronization.
    kStartedWatching,

    //! \brief A `WM_ENDSESSION` or `WM_CLOSE` message was received.
    kSessionEnding,

    //! \brief Monitoring for messages has ended.
    //!
    //! Because the message loop runs on a different thread than the object was
    //! created on, this event allows for synchronization.
    kStoppedWatching,
  };

  //! \brief Notification of an event.
  //!
  //! This method is called on the thread that runs the message loop.
  virtual void SessionEndWatcherEvent(Event event) = 0;

  // Exposed for testing.
  HWND GetWindow() const { return window_; }

 private:
  static DWORD WINAPI ThreadMain(void* argument);

  static LRESULT CALLBACK WindowProc(HWND window,
                                     UINT message,
                                     WPARAM w_param,
                                     LPARAM l_param);

  ScopedKernelHANDLE thread_;
  HWND window_;  // Conceptually strong, but ownership managed in ThreadMain()

  DISALLOW_COPY_AND_ASSIGN(SessionEndWatcher);
};

}  // namespace crashpad

#endif  // CRASHPAD_UTIL_WIN_SESSION_END_WATCHER_H_
