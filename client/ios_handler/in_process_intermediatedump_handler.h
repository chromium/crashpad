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

#include "base/stl_util.h"
#include "util/ios/ios_intermediatedump_writer.h"
#include "util/ios/ios_system_data_collector.h"
#include "util/mach/mach_extensions.h"

#ifndef CRASHPAD_CLIENT_IOS_HANDLER_IN_PROCESS_INTERMEDIATEDUMP_HANDLER_H_
#define CRASHPAD_CLIENT_IOS_HANDLER_IN_PROCESS_INTERMEDIATEDUMP_HANDLER_H_

namespace crashpad {
namespace internal {

class InProcessIntermediatedumpHandler final {
 public:
  static void WriteHeader(IOSIntermediatedumpWriter* writer);
  static void WriteProcessInfo(IOSIntermediatedumpWriter* writer);
  static void WriteSystemInfo(IOSIntermediatedumpWriter* writer,
                              const IOSSystemDataCollector& system_data);
  static void WriteThreadInfo(IOSIntermediatedumpWriter* writer);
  static void WriteModuleInfo(IOSIntermediatedumpWriter* writer);
  static void WriteModuleInfoAtAddress(IOSIntermediatedumpWriter* writer,
                                       uint64_t address);
  static void WriteExceptionFromSignal(
      IOSIntermediatedumpWriter* writer,
      const IOSSystemDataCollector& system_data,
      siginfo_t* siginfo,
      ucontext_t* context);
  static void WriteMachExceptionInfo(IOSIntermediatedumpWriter* writer,
                                     exception_behavior_t behavior,
                                     thread_t thread,
                                     exception_type_t exception,
                                     const mach_exception_data_type_t* code,
                                     mach_msg_type_number_t code_count,
                                     thread_state_flavor_t flavor,
                                     ConstThreadState old_state,
                                     mach_msg_type_number_t old_state_count);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(InProcessIntermediatedumpHandler);
};

}  // namespace internal
}  // namespace crashpad

#endif  // CRASHPAD_CLIENT_IOS_HANDLER_IN_PROCESS_INTERMEDIATEDUMP_HANDLER_H_
