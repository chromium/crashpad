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

#include "util/thread/thread.h"

#include <errno.h>
#include <sys/mman.h>

#include "base/logging.h"
#include "base/process/process_metrics.h"

namespace crashpad {

void Thread::Start() {
  DCHECK(!platform_thread_);

  if (guarded_stack_page_size_ > 0) {
    pthread_attr_t attr;
    errno = pthread_attr_init(&attr);
    PLOG_IF(WARNING, errno != 0) << "pthread_attr_init";

    errno = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    PLOG_IF(WARNING, errno != 0) << "pthread_attr_setdetachstate";

    int guarded_size = guarded_stack_page_size_ + 2;
    guarded_stack_ = mmap(nullptr,
                          base::GetPageSize() * guarded_size,
                          PROT_READ | PROT_WRITE,
                          MAP_PRIVATE | MAP_ANONYMOUS,
                          -1,
                          0);
    PCHECK(guarded_stack_ != MAP_FAILED) << "mmap";

    // mprotect the first and last page.
    errno = mprotect(guarded_stack_, base::GetPageSize(), PROT_NONE);
    PLOG_IF(WARNING, errno != 0) << "mprotect";

    errno = mprotect(
        reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(guarded_stack_) +
                                base::GetPageSize() * (guarded_size - 1)),
        base::GetPageSize(),
        PROT_NONE);
    PLOG_IF(WARNING, errno != 0) << "mprotect";

    errno = pthread_attr_setstack(
        &attr,
        reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(guarded_stack_) +
                                base::GetPageSize()),
        base::GetPageSize() * guarded_stack_page_size_);
    PLOG_IF(WARNING, errno != 0) << "pthread_attr_setstack";

    errno = pthread_create(&platform_thread_, &attr, ThreadEntryThunk, this);
  } else {
    errno = pthread_create(&platform_thread_, 0, ThreadEntryThunk, this);
  }
  PCHECK(errno == 0) << "pthread_create";
}

void Thread::Join() {
  DCHECK(platform_thread_);
  errno = pthread_join(platform_thread_, nullptr);
  PCHECK(errno == 0) << "pthread_join";
  platform_thread_ = 0;
}

// static
void* Thread::ThreadEntryThunk(void* argument) {
  Thread* self = reinterpret_cast<Thread*>(argument);
  self->ThreadMain();
  return nullptr;
}

}  // namespace crashpad
