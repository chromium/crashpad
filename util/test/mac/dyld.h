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

#ifndef CRASHPAD_UTIL_TEST_MAC_DYLD_H_
#define CRASHPAD_UTIL_TEST_MAC_DYLD_H_

#include <mach-o/dyld_images.h>

extern "C" {

// Returns a pointer to this process’ dyld_all_image_infos structure. This is
// implemented as a non-public dyld API, declared in 10.9.2
// dyld-239.4/include/mach-o/dyld_priv.h.
const struct dyld_all_image_infos* _dyld_get_all_image_infos();

}  // extern "C"

#endif  // CRASHPAD_UTIL_TEST_MAC_DYLD_H_
