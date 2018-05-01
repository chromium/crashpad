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

#include <stdio.h>
#include <zircon/process.h>
#include <zircon/syscalls.h>

int main() {
  printf("crasher sleeping for 1s...\n");
  zx_nanosleep(zx_deadline_after(ZX_SEC(1)));

  zx_koid_t koid = ZX_KOID_INVALID;
  zx_info_handle_basic_t basic;
  zx_status_t status = zx_object_get_info(zx_process_self(),
                                          ZX_INFO_HANDLE_BASIC,
                                          &basic,
                                          sizeof(basic),
                                          nullptr,
                                          nullptr);
  if (status != ZX_OK) {
    printf("zx_object_get_info failed?");
  }
  koid = basic.koid;

  printf("pid=%lu about to crash...\n", koid);
  *(volatile int*)42 = 3;

  printf("should have crashed and not got here\n");
  zx_nanosleep(zx_deadline_after(ZX_SEC(1)));
  return 0;
}
