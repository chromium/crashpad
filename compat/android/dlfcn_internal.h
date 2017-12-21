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

#ifndef CRASHPAD_COMPAT_ANDROID_DLFCN_INTERNAL_H_
#define CRASHPAD_COMPAT_ANDROID_DLFCN_INTERNAL_H_

namespace crashpad {
namespace internal {

// dlsym on Android KitKat (4.4.*) raises SIGFPE when searching for a
// non-existent symbol. This wrapper avoids crashing in this circumstance.
void* Dlsym(void* handle, const char* symbol);

}  // namespace internal
}  // namespace crashpad

#endif  // CRASHPAD_COMPAT_ANDROID_DLFCN_INTERNAL_H_
