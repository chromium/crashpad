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

#include "test/mac/dyld.h"

#include <AvailabilityMacros.h>
#include <dlfcn.h>
#include <mach/mach.h>
#include <mach-o/dyld.h>
#include <stdint.h>

#include "base/logging.h"
#include "base/mac/mach_logging.h"
#include "build/build_config.h"
#include "test/dl_handle.h"
#include "util/numeric/safe_assignment.h"

#if MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_13
extern "C" {

// A non-public dyld API, declared in 10.12.4
// dyld-433.5/include/mach-o/dyld_priv.h. The code still exists in 10.13, but
// its symbol is no longer public, so it can’t be used there.
const dyld_all_image_infos* _dyld_get_all_image_infos()
    __attribute__((weak_import));

}  // extern "C"
#endif

namespace crashpad {
namespace test {

const dyld_all_image_infos* DyldGetAllImageInfos() {
#if MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_13
  // If the old interface is available (both in the SDK and at run time), use
  // it.
  if (_dyld_get_all_image_infos) {
    return _dyld_get_all_image_infos();
  }
#elif MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_13
  // There’s no _dyld_get_all_image_infos symbol to link against in the SDK, but
  // this may run on an older system that provides that symbol at run time. If
  // it’s present, it’ll be in the same module as _dyld_image_count, so look for
  // it there and use it if it’s found.
  Dl_info dli;
  if (!dladdr(reinterpret_cast<void*>(_dyld_image_count), &dli)) {
    LOG(WARNING) << "dladdr: failed";
  } else {
    ScopedDlHandle dlh(
        dlopen(dli.dli_fname, RTLD_LAZY | RTLD_LOCAL | RTLD_NOLOAD));
    if (!dlh.valid()) {
      LOG(WARNING) << "dlopen: " << dlerror();
    } else {
      using DyldGetAllImageInfosType = const dyld_all_image_infos*();
      const auto _dyld_get_all_image_infos =
          dlh.LookUpSymbol<DyldGetAllImageInfosType*>(
              "_dyld_get_all_image_infos");
      if (_dyld_get_all_image_infos) {
        return _dyld_get_all_image_infos();
      }
    }
  }
#endif

  task_dyld_info_data_t dyld_info;
  mach_msg_type_number_t count = TASK_DYLD_INFO_COUNT;
  kern_return_t kr = task_info(mach_task_self(),
                               TASK_DYLD_INFO,
                               reinterpret_cast<task_info_t>(&dyld_info),
                               &count);
  if (kr != KERN_SUCCESS) {
    MACH_LOG(ERROR, kr) << "task_info";
    return nullptr;
  }

  if (count < TASK_DYLD_INFO_COUNT) {
    LOG(ERROR) << "unexpected task_dyld_info_data_t::count " << count;
    return nullptr;
  }

#if MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_7
  // The task_dyld_info_data_t struct grew in 10.7, adding the format field.
  // Don’t check this field if it’s not present, which can happen when the SDK
  // used at compile time is too old and doesn’t know about it. The kernel at
  // run time may also be too old to populate that field, but in that case,
  // _dyld_get_all_image_infos() would be available and would have been used
  // above. (Otherwise, the “count” check above would be triggered.)
#if defined(ARCH_CPU_32_BITS)
  constexpr integer_t kExpectedFormat = TASK_DYLD_ALL_IMAGE_INFO_32;
#elif defined(ARCH_CPU_64_BITS)
  constexpr integer_t kExpectedFormat = TASK_DYLD_ALL_IMAGE_INFO_64;
#endif

  if (dyld_info.all_image_info_format != kExpectedFormat) {
    LOG(ERROR) << "unexpected task_dyld_info_data_t::all_image_info_format "
               << dyld_info.all_image_info_format;
    return nullptr;
  }
#endif

  uintptr_t all_image_info_addr;
  if (!AssignIfInRange(&all_image_info_addr, dyld_info.all_image_info_addr)) {
    LOG(ERROR) << "task_dyld_info_data_t::all_image_info_addr "
               << dyld_info.all_image_info_addr << " out of range";
    return nullptr;
  }

  return reinterpret_cast<const dyld_all_image_infos*>(all_image_info_addr);
}

}  // namespace test
}  // namespace crashpad
