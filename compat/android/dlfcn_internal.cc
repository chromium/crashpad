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

#include "dlfcn_internal.h"

#include <android/api-level.h>
#include <dlfcn.h>
#include <errno.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/system_properties.h>
#include <unistd.h>

#include <mutex>

namespace crashpad {
namespace internal {

// KitKat supports API levels up to 20.
#if __ANDROID_API__ < 21

namespace {

class ScopedSigactionRestore {
 public:
  ScopedSigactionRestore() : old_action_(), signo_(-1), valid_(false) {}

  ~ScopedSigactionRestore() { Reset(); }

  bool Reset() {
    bool result = true;
    if (valid_) {
      result = sigaction(signo_, &old_action_, nullptr) == 0;
      if (!result) {
        PrintErrmsg(errno);
      }
    }
    valid_ = false;
    signo_ = -1;
    return result;
  }

  bool ResetInstallHandler(int signo, void (*handler)(int)) {
    Reset();

    struct sigaction act;
    act.sa_handler = handler;
    sigemptyset(&act.sa_mask);
    act.sa_flags = SA_NODEFER;
    if (sigaction(signo, &act, &old_action_) != 0) {
      PrintErrmsg(errno);
      return false;
    }
    signo_ = signo;
    valid_ = true;
    return true;
  }

 private:
  void PrintErrmsg(int err) {
    char errmsg[256];

    if (strerror_r(err, errmsg, sizeof(errmsg)) != 0) {
      snprintf(
          errmsg, sizeof(errmsg), "Couldn't set errmsg for %d: %d", err, errno);
      return;
    }

    fprintf(stderr, "sigaction %s", errmsg);
  }

  struct sigaction old_action_;
  int signo_;
  bool valid_;
};

bool IsKitKat() {
  char prop_buf[PROP_VALUE_MAX];
  int length = __system_property_get("ro.build.version.sdk", prop_buf);
  if (length <= 0) {
    fprintf(stderr, "Couldn't get version");
    // It's safer to assume this is KitKat and execute dlsym with a signal
    // handler installed.
    return true;
  }
  static constexpr char kitkat_version[] = "19";
  if (strcmp(prop_buf, kitkat_version) == 0) {
    return true;
  }
  return false;
}

class ScopedLock {
 public:
  ScopedLock(std::mutex* mutex) : mutex_(mutex) { mutex_->lock(); }

  ~ScopedLock() { mutex_->unlock(); }

 private:
  std::mutex* mutex_;
};

sigjmp_buf dlsym_sigjmp_env;

bool in_handler = false;

void HandleSIGFPE(int signo) {
  if (in_handler) {
    _exit(1);
  }
  in_handler = true;
  siglongjmp(dlsym_sigjmp_env, 1);
}

std::mutex* signal_handler_mutex = new std::mutex();

}  // namespace

void* Dlsym(void* handle, const char* symbol) {
  if (!IsKitKat()) {
    return dlsym(handle, symbol);
  }

  ScopedLock lock(signal_handler_mutex);
  ScopedSigactionRestore sig_restore;
  if (!sig_restore.ResetInstallHandler(SIGFPE, HandleSIGFPE)) {
    return nullptr;
  }

  if (sigsetjmp(dlsym_sigjmp_env, 1) != 0) {
    in_handler = false;
    return nullptr;
  }

  return dlsym(handle, symbol);
}

#else

void* Dlsym(void* handle, const char* symbol) {
  return dlsym(handle, symbol);
}

#endif  // __ANDROID_API__ < 21

}  // namespace internal
}  // namespace crashpad
