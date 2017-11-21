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

#include <sys/epoll.h>

#include <unistd.h>
#include <sys/syscall.h>

#if __ANDROID_API_ < 21

extern "C" {

int epoll_create1(int flags) {
  return syscall(SYS_epoll_create1, flags);
}

}  // extern "C"

#endif  // __ANDROID_API_ < 21
