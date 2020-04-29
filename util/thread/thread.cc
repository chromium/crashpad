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

#include <sys/mman.h>

#include "base/logging.h"
#include "base/process/process_metrics.h"

namespace crashpad {

Thread::Thread() : platform_thread_(0), guarded_stack_page_size_(0) {}

#if defined(OS_POSIX)
Thread::Thread(size_t guarded_stack_page_size)
    : platform_thread_(0), guarded_stack_page_size_(guarded_stack_page_size) {}
#endif

Thread::~Thread() {
  DCHECK(!platform_thread_);

#if defined(OS_POSIX)
  PCHECK(munmap(guarded_stack_,
                base::GetPageSize() * guarded_stack_page_size_) >= 0)
      << "munmap";

#endif
}

}  // namespace crashpad
