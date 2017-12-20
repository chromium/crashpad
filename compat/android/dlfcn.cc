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

#include <dlfcn.h>

#include <errno.h>
#include <pthread.h>
#include <signal.h>

#include "base/logging.h"
#include "base/macros.h"

namespace crashpad {

namespace {

class ScopedSigactionRestore {
 public:
  ScopedSigactionRestore() : old_action_(), signo_(-1), valid_(false) {}

  ~ScopedSigactionRestore() { Reset(); }

  bool Reset() {
    if (valid_) {
      int res = sigaction(signo_, &old_action_, nullptr);
      if (res != 0) {
        PLOG(ERROR) << "sigaction";
        return false;
      }
    }
    valid_ = false;
    signo_ = -1;
    return true;
  }

  bool ResetInstallHandler(int signo, void (*handler)(int)) {
    if (!Reset()) {
      return false;
    }

    struct sigaction act;
    act.sa_handler = handler;
    sigemptyset(&act.sa_mask);
    act.sa_flags = SA_NODEFER;
    if (sigaction(signo, &act, &old_action_) != 0) {
      PLOG(ERROR) << "sigaction";
      return false;
    }
    signo_ = signo;
    valid_ = true;
    return true;
  }

 private:
  struct sigaction old_action_;
  int signo_;
  bool valid_;

  DISALLOW_COPY_AND_ASSIGN(ScopedSigactionRestore);
};

void handle_sigfpe(int signo) {
  pthread_exit(nullptr);
}

void* find_symbol(void* name) {
  ScopedSigactionRestore sig_restore;
  if (!sig_restore.ResetInstallHandler(SIGFPE, handle_sigfpe)) {
    return nullptr;
  }
  return dlsym(RTLD_DEFAULT, reinterpret_cast<const char*>(name));
}

}  // namespace

void* Dlsym(void* handle, const char* symbol) {
  pthread_t thread;
  errno = pthread_create(&thread,
                         nullptr,
                         find_symbol,
                         reinterpret_cast<void*>(const_cast<char*>(symbol)));
  if (errno != 0) {
    PLOG(ERROR) << "pthread_create";
    return nullptr;
  }

  void* sym_ptr;
  errno = pthread_join(thread, &sym_ptr);
  if (errno != 0) {
    PLOG(ERROR) << "pthread_join";
    return nullptr;
  }

  return sym_ptr;
}

}  // namespace crashpad
