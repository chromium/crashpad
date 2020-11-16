// Copyright 2020 The Crashpad Authors. All rights reserved.
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

#include <fcntl.h>
#include <mach-o/dyld_images.h>
#include <mach-o/loader.h>
#include <mach/mach.h>
#include <sys/sysctl.h>
#include <unistd.h>

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <uuid/uuid.h>

#include "base/stl_util.h"
#include "util/ios/ios_system_data_collector.h"
#include "util/ios/pack_ios_state.h"
#include "util/mach/mach_extensions.h"
#include "util/misc/from_pointer_cast.h"

namespace crashpad {
namespace internal {

void WriteHeader(int fd);
void WriteProcessInfo(int fd);
void WriteSystemInfo(int fd, const IOSSystemDataCollector& system_data);
void WriteThreadInfo(int fd);
void WriteModuleInfo(int fd);
void WriteModuleInfoAtAddress(int fd, uint64_t address);
void WriteExceptionFromSignal(int fd,
                              const IOSSystemDataCollector& system_data,
                              siginfo_t* siginfo,
                              ucontext_t* context);
void WriteMachExceptionInfo(int fd,
                            exception_behavior_t behavior,
                            thread_t thread,
                            exception_type_t exception,
                            const mach_exception_data_type_t* code,
                            mach_msg_type_number_t code_count,
                            thread_state_flavor_t flavor,
                            ConstThreadState old_state,
                            mach_msg_type_number_t old_state_count);

}  // namespace internal
}  // namespace crashpad
