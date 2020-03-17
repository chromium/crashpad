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

#ifndef CRASHPAD_SNAPSHOT_IOS_CPU_CONTEXT_IOS_H_
#define CRASHPAD_SNAPSHOT_IOS_CPU_CONTEXT_IOS_H_

#include <mach/mach.h>

#include "build/build_config.h"
#include "snapshot/cpu_context.h"

namespace crashpad {
namespace internal {

#if defined(ARCH_CPU_X86_64)
//! \brief Initializes a CPUContextX86_64 structure from native context
//! structures
//!     on iOS.
//!
//! \param[out] context The CPUContextX86 structure to initialize.
//! \param[in] x86_thread_state32 The state of the thread’s integer registers.
//! \param[in] x86_float_state32 The state of the thread’s floating-point
//!     registers.
//! \param[in] x86_debug_state32 The state of the thread’s debug registers.
void InitializeCPUContext(CPUContextX86_64* context,
                          const x86_thread_state64_t* x86_thread_state64,
                          const x86_float_state64_t* x86_float_state64,
                          const x86_debug_state64_t* x86_debug_state64);

#elif defined(ARCH_CPU_ARM64)
//! \brief Initializes a CPUContextARM64 structure from native context
//! structures
//!     on iOS.
//!
//! \param[out] context The CPUContextARM64 structure to initialize.
//! \param[in] arm_thread_state64 The state of the thread’s integer registers.
//! \param[in] arm_neon_state64 The state of the thread’s floating-point
//!     registers.
void InitializeCPUContext(CPUContextARM64* type_context,
                          arm_thread_state64_t* arm_thread_state64,
                          arm_neon_state64_t* arm_neon_state64);

#else
#error Port.
#endif  // ARCH_CPU_X86_64

}  // namespace internal
}  // namespace crashpad

#endif  // CRASHPAD_SNAPSHOT_IOS_CPU_CONTEXT_IOS_H_
