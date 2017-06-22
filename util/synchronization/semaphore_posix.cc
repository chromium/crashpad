// Copyright 2014 The Crashpad Authors. All rights reserved.
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

#include "util/synchronization/semaphore.h"

#include <errno.h>
#include <math.h>
#include <time.h>

#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"

namespace crashpad {

#if !defined(OS_MACOSX)

namespace {

void AddTimespec(const timespec& ts1, const timespec& ts2, timespec* result) {
  result->tv_sec = ts1.tv_sec + ts2.tv_sec;
  result->tv_nsec = ts1.tv_nsec + ts2.tv_nsec;
  if (result->tv_nsec > static_cast<long>(1E9)) {
    ++result->tv_sec;
    result->tv_nsec -= static_cast<long>(1E9);
  }
}

}  // namespace

Semaphore::Semaphore(int value) {
  PCHECK(sem_init(&semaphore_, 0, value) == 0) << "sem_init";
}

Semaphore::~Semaphore() {
  PCHECK(sem_destroy(&semaphore_) == 0) << "sem_destroy";
}

void Semaphore::Wait() {
  PCHECK(HANDLE_EINTR(sem_wait(&semaphore_)) == 0) << "sem_wait";
}

bool Semaphore::TimedWait(double seconds) {
  DCHECK_GE(seconds, 0.0);

  if (isinf(seconds)) {
    Wait();
    return true;
  }

  timespec current_time;
  if (clock_gettime(CLOCK_REALTIME, &current_time) != 0) {
    PLOG(ERROR) << "clock_gettime";
    return false;
  }
  timespec timeout;
  timeout.tv_sec = seconds;
  timeout.tv_nsec = (seconds - trunc(seconds)) * 1E9;
  AddTimespec(current_time, timeout, &timeout);

  int rv = HANDLE_EINTR(sem_timedwait(&semaphore_, &timeout));
  PCHECK(rv == 0 || errno == ETIMEDOUT) << "sem_timedwait";
  return rv == 0;
}

void Semaphore::Signal() {
  PCHECK(sem_post(&semaphore_) == 0) << "sem_post";
}

#endif

}  // namespace crashpad
