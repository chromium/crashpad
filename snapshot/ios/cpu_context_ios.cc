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

#include "snapshot/ios/cpu_context_ios.h"

#if defined(ARCH_CPU_X86_64)
#include "snapshot/mac/cpu_context_mac.h"
#endif

namespace crashpad {
namespace internal {

#if defined(ARCH_CPU_X86_64)
void InitializeCPUContext(CPUContextX86_64* context,
                          const x86_thread_state64_t* x86_thread_state64,
                          const x86_float_state64_t* x86_float_state64,
                          const x86_debug_state64_t* x86_debug_state64) {
  // Implementation is shared with macOS.
  InitializeCPUContextX86_64(context,
                             THREAD_STATE_NONE,
                             nullptr,
                             0,
                             x86_thread_state64,
                             x86_float_state64,
                             x86_debug_state64);
}
#elif defined(ARCH_CPU_ARM64)
void InitializeCPUContext(CPUContextARM64* context,
                          arm_thread_state64_t* arm_thread_state64,
                          arm_neon_state64_t* arm_neon_state64) {
  //   __uint64_t __x[29];     /* General purpose registers x0-x28 */
  //  void*      __opaque_fp; /* Frame pointer x29 */
  //  void*      __opaque_lr; /* Link register x30 */
  //  void*      __opaque_sp; /* Stack pointer x31 */
  //  static_assert(sizeof(context->regs) == sizeof(arm_thread_state64->__x),
  //                "registers size mismatch");
  // Is below safe?
  memcpy(context->regs, arm_thread_state64->__x, sizeof(context->regs));
  context->sp = arm_thread_state64->__sp;
  context->pc = arm_thread_state64->__pc;
  context->spsr =
      static_cast<decltype(context->spsr)>(arm_thread_state64->__cpsr);

  static_assert(sizeof(context->fpsimd) == sizeof(arm_neon_state64->__v),
                "fpsimd context size mismatch");
  memcpy(context->fpsimd, arm_neon_state64->__v, sizeof(arm_neon_state64->__v));
  context->fpsr = arm_neon_state64->__fpsr;
  context->fpcr = arm_neon_state64->__fpcr;
}

#endif

}  // namespace internal
}  // namespace crashpad
