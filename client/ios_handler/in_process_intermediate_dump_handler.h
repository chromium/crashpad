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

#include <mach-o/loader.h>

#include "base/cxx17_backports.h"
#include "snapshot/mac/process_types.h"
#include "util/ios/ios_intermediate_dump_writer.h"
#include "util/ios/ios_system_data_collector.h"
#include "util/mach/mach_extensions.h"

#ifndef CRASHPAD_CLIENT_IOS_HANDLER_IN_PROCESS_INTERMEDIATE_DUMP_HANDLER_H_
#define CRASHPAD_CLIENT_IOS_HANDLER_IN_PROCESS_INTERMEDIATE_DUMP_HANDLER_H_

namespace crashpad {
namespace internal {

#if defined(ARCH_CPU_X86_64)
using thread_state_type = x86_thread_state64_t;
#elif defined(ARCH_CPU_ARM64)
using thread_state_type = arm_thread_state64_t;
#endif

class InProcessIntermediateDumpHandler final {
 public:
  static void WriteHeader(IOSIntermediateDumpWriter* writer);
  static void WriteProcessInfo(IOSIntermediateDumpWriter* writer);
  static void WriteSystemInfo(IOSIntermediateDumpWriter* writer,
                              const IOSSystemDataCollector& system_data);
  static void WriteThreadInfo(IOSIntermediateDumpWriter* writer,
                              const uint64_t* frames,
                              const size_t num_frames);
  static void WriteModuleInfo(IOSIntermediateDumpWriter* writer);
  static void WriteModuleInfoAtAddress(IOSIntermediateDumpWriter* writer,
                                       uint64_t address,
                                       bool isDyld = false);
  static void WriteExceptionFromSignal(
      IOSIntermediateDumpWriter* writer,
      const IOSSystemDataCollector& system_data,
      siginfo_t* siginfo,
      ucontext_t* context);
  static void WriteMachExceptionInfo(IOSIntermediateDumpWriter* writer,
                                     exception_behavior_t behavior,
                                     thread_t thread,
                                     exception_type_t exception,
                                     const mach_exception_data_type_t* code,
                                     mach_msg_type_number_t code_count,
                                     thread_state_flavor_t flavor,
                                     ConstThreadState old_state,
                                     mach_msg_type_number_t old_state_count);
  static void WriteNSException(IOSIntermediateDumpWriter* writer);

 private:
  static void WriteDataAnnotations(IOSIntermediateDumpWriter* writer,
                                   const segment_command_64* segment,
                                   vm_size_t slide);
  static void WriteAnnotationList(IOSIntermediateDumpWriter* writer,
                                  process_types::CrashpadInfo* crashpad_info);
  static void WriteSimpleAnnotation(IOSIntermediateDumpWriter* writer,
                                    process_types::CrashpadInfo* crashpad_info);
  static void WriteCrashInfoAnnotations(
      IOSIntermediateDumpWriter* writer,
      process_types::crashreporter_annotations_t* crash_info);
  static void WriteDyldErrorStringAnnotation(
      IOSIntermediateDumpWriter* writer,
      const uint64_t address,
      const symtab_command* symtab_command,
      const dysymtab_command* dysymtab_command,
      const segment_command_64* text_seg,
      const segment_command_64* linkedit_seg,
      vm_size_t slide);
  static void CaptureMemoryPointedToByThreadState(
      IOSIntermediateDumpWriter* writer,
      thread_state_type thread_state);
  static void MaybeCaptureMemoryAround(IOSIntermediateDumpWriter* writer,
                                       uint64_t address);
  DISALLOW_IMPLICIT_CONSTRUCTORS(InProcessIntermediateDumpHandler);
};

}  // namespace internal
}  // namespace crashpad

#endif  // CRASHPAD_CLIENT_IOS_HANDLER_IN_PROCESS_INTERMEDIATE_DUMP_HANDLER_H_
