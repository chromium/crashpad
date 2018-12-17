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

#include <android/api-level.h>

#include <limits.h>
#include <stdlib.h>
#include <sys/system_properties.h>

#if __ANDROID_API__ < 29

extern "C" {

int android_get_device_api_level() {
  char api_string[PROP_VALUE_MAX];
  int length = __system_property_get("ro.build.version.sdk", api_string);
  if (length <= 0) {
    return -1;
  }

  char* endptr;
  long api_level = strtol(api_string, &endptr, 10);
  return api_level > 0 && api_level < INT_MAX && *endptr == '\0' ? api_level
                                                                 : -1;
}

}  // extern "C"

#endif  // __ANDROID_API__ < 29
