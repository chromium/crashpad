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

#include "snapshot/test/test_cpu_context.h"

#include "base/basictypes.h"

namespace crashpad {
namespace test {

void InitializeCPUContextX86(CPUContext* context, uint32_t seed) {
  context->architecture = kCPUArchitectureX86;

  if (seed == 0) {
    memset(context->x86, 0, sizeof(*context->x86));
    return;
  }

  uint32_t value = seed;

  context->x86->eax = value++;
  context->x86->ebx = value++;
  context->x86->ecx = value++;
  context->x86->edx = value++;
  context->x86->edi = value++;
  context->x86->esi = value++;
  context->x86->ebp = value++;
  context->x86->esp = value++;
  context->x86->eip = value++;
  context->x86->eflags = value++;
  context->x86->cs = value++;
  context->x86->ds = value++;
  context->x86->es = value++;
  context->x86->fs = value++;
  context->x86->gs = value++;
  context->x86->ss = value++;
  InitializeCPUContextX86Fxsave(&context->x86->fxsave, &value);
  context->x86->dr0 = value++;
  context->x86->dr1 = value++;
  context->x86->dr2 = value++;
  context->x86->dr3 = value++;
  context->x86->dr4 = value++;
  context->x86->dr5 = value++;
  context->x86->dr6 = value++;
  context->x86->dr7 = value++;
}

void InitializeCPUContextX86_64(CPUContext* context, uint32_t seed) {
  context->architecture = kCPUArchitectureX86_64;

  if (seed == 0) {
    memset(context->x86_64, 0, sizeof(*context->x86_64));
    return;
  }

  uint32_t value = seed;

  context->x86_64->rax = value++;
  context->x86_64->rbx = value++;
  context->x86_64->rcx = value++;
  context->x86_64->rdx = value++;
  context->x86_64->rdi = value++;
  context->x86_64->rsi = value++;
  context->x86_64->rbp = value++;
  context->x86_64->rsp = value++;
  context->x86_64->r8 = value++;
  context->x86_64->r9 = value++;
  context->x86_64->r10 = value++;
  context->x86_64->r11 = value++;
  context->x86_64->r12 = value++;
  context->x86_64->r13 = value++;
  context->x86_64->r14 = value++;
  context->x86_64->r15 = value++;
  context->x86_64->rip = value++;
  context->x86_64->rflags = value++;
  context->x86_64->cs = value++;
  context->x86_64->fs = value++;
  context->x86_64->gs = value++;
  InitializeCPUContextX86_64Fxsave(&context->x86_64->fxsave, &value);
  context->x86_64->dr0 = value++;
  context->x86_64->dr1 = value++;
  context->x86_64->dr2 = value++;
  context->x86_64->dr3 = value++;
  context->x86_64->dr4 = value++;
  context->x86_64->dr5 = value++;
  context->x86_64->dr6 = value++;
  context->x86_64->dr7 = value++;
}

namespace {

// This is templatized because the CPUContextX86::Fxsave and
// CPUContextX86_64::Fxsave are nearly identical but have different sizes for
// the members |xmm|, |reserved_4|, and |available|.
template <typename FxsaveType>
void InitializeCPUContextFxsave(FxsaveType* fxsave, uint32_t* seed) {
  uint32_t value = *seed;

  fxsave->fcw = value++;
  fxsave->fsw = value++;
  fxsave->ftw = value++;
  fxsave->reserved_1 = value++;
  fxsave->fop = value++;
  fxsave->fpu_ip = value++;
  fxsave->fpu_cs = value++;
  fxsave->reserved_2 = value++;
  fxsave->fpu_dp = value++;
  fxsave->fpu_ds = value++;
  fxsave->reserved_3 = value++;
  fxsave->mxcsr = value++;
  fxsave->mxcsr_mask = value++;
  for (size_t st_mm_index = 0;
       st_mm_index < arraysize(fxsave->st_mm);
       ++st_mm_index) {
    for (size_t byte = 0;
         byte < arraysize(fxsave->st_mm[st_mm_index].st);
         ++byte) {
      fxsave->st_mm[st_mm_index].st[byte] = value++;
    }
    for (size_t byte = 0;
         byte < arraysize(fxsave->st_mm[st_mm_index].st_reserved);
         ++byte) {
      fxsave->st_mm[st_mm_index].st_reserved[byte] = value;
    }
  }
  for (size_t xmm_index = 0; xmm_index < arraysize(fxsave->xmm); ++xmm_index) {
    for (size_t byte = 0; byte < arraysize(fxsave->xmm[xmm_index]); ++byte) {
      fxsave->xmm[xmm_index][byte] = value++;
    }
  }
  for (size_t byte = 0; byte < arraysize(fxsave->reserved_4); ++byte) {
    fxsave->reserved_4[byte] = value++;
  }
  for (size_t byte = 0; byte < arraysize(fxsave->available); ++byte) {
    fxsave->available[byte] = value++;
  }

  *seed = value;
}

}  // namespace

void InitializeCPUContextX86Fxsave(CPUContextX86::Fxsave* fxsave,
                                   uint32_t* seed) {
  return InitializeCPUContextFxsave(fxsave, seed);
}

void InitializeCPUContextX86_64Fxsave(CPUContextX86_64::Fxsave* fxsave,
                                      uint32_t* seed) {
  return InitializeCPUContextFxsave(fxsave, seed);
}

}  // namespace test
}  // namespace crashpad
