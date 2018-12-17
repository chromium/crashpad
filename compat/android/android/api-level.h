// Copyright 2018 The Crashpad Authors. All rights reserved.
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

#ifndef CRASHPAD_COMPAT_ANDROID_ANDROID_API_LEVEL_H_
#define CRASHPAD_COMPAT_ANDROID_ANDROID_API_LEVEL_H_

#include_next <android/api-level.h>

#include <sys/cdefs.h>

#if __ANDROID_API__ < 29

#ifdef __cplusplus
extern "C" {
#endif

// Returns the API level of the device or -1 if it can't be determined. This
// function is provided by Bionic at API 29.
int android_get_device_api_level();

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // __ANDROID_API__ < 29

#endif  // CRASHPAD_COMPAT_ANDROID_ANDROID_API_LEVEL_H_
