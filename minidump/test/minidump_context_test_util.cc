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

#include "minidump/test/minidump_context_test_util.h"

#include <string.h>
#include <sys/types.h>

#include "base/format_macros.h"
#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "gtest/gtest.h"
#include "snapshot/cpu_context.h"
#include "snapshot/test/test_cpu_context.h"
#include "test/hex_string.h"

namespace crashpad {
namespace test {

void InitializeMinidumpContextX86(MinidumpContextX86* context, uint32_t seed) {
  if (seed == 0) {
    memset(context, 0, sizeof(*context));
    context->context_flags = kMinidumpContextX86;
    return;
  }

  context->context_flags = kMinidumpContextX86All;

  uint32_t value = seed;

  context->eax = value++;
  context->ebx = value++;
  context->ecx = value++;
  context->edx = value++;
  context->edi = value++;
  context->esi = value++;
  context->ebp = value++;
  context->esp = value++;
  context->eip = value++;
  context->eflags = value++;
  context->cs = value++ & 0xffff;
  context->ds = value++ & 0xffff;
  context->es = value++ & 0xffff;
  context->fs = value++ & 0xffff;
  context->gs = value++ & 0xffff;
  context->ss = value++ & 0xffff;

  InitializeCPUContextX86Fxsave(&context->fxsave, &value);
  CPUContextX86::FxsaveToFsave(context->fxsave, &context->fsave);

  context->dr0 = value++;
  context->dr1 = value++;
  context->dr2 = value++;
  context->dr3 = value++;
  value += 2;  // Minidumps don’t carry dr4 or dr5.
  context->dr6 = value++;
  context->dr7 = value++;

  // Set this field last, because it has no analogue in CPUContextX86.
  context->float_save.spare_0 = value++;
}

void InitializeMinidumpContextAMD64(MinidumpContextAMD64* context,
                                    uint32_t seed) {
  if (seed == 0) {
    memset(context, 0, sizeof(*context));
    context->context_flags = kMinidumpContextAMD64;
    return;
  }

  context->context_flags = kMinidumpContextAMD64All;

  uint32_t value = seed;

  context->rax = value++;
  context->rbx = value++;
  context->rcx = value++;
  context->rdx = value++;
  context->rdi = value++;
  context->rsi = value++;
  context->rbp = value++;
  context->rsp = value++;
  context->r8 = value++;
  context->r9 = value++;
  context->r10 = value++;
  context->r11 = value++;
  context->r12 = value++;
  context->r13 = value++;
  context->r14 = value++;
  context->r15 = value++;
  context->rip = value++;
  context->eflags = value++;
  context->cs = static_cast<uint16_t>(value++);
  context->fs = static_cast<uint16_t>(value++);
  context->gs = static_cast<uint16_t>(value++);

  InitializeCPUContextX86_64Fxsave(&context->fxsave, &value);

  // mxcsr appears twice, and the two values should be aliased.
  context->mx_csr = context->fxsave.mxcsr;

  context->dr0 = value++;
  context->dr1 = value++;
  context->dr2 = value++;
  context->dr3 = value++;
  value += 2;  // Minidumps don’t carry dr4 or dr5.
  context->dr6 = value++;
  context->dr7 = value++;

  // Set these fields last, because they have no analogues in CPUContextX86_64.
  context->p1_home = value++;
  context->p2_home = value++;
  context->p3_home = value++;
  context->p4_home = value++;
  context->p5_home = value++;
  context->p6_home = value++;
  context->ds = static_cast<uint16_t>(value++);
  context->es = static_cast<uint16_t>(value++);
  context->ss = static_cast<uint16_t>(value++);
  for (size_t index = 0; index < arraysize(context->vector_register); ++index) {
    context->vector_register[index].lo = value++;
    context->vector_register[index].hi = value++;
  }
  context->vector_control = value++;
  context->debug_control = value++;
  context->last_branch_to_rip = value++;
  context->last_branch_from_rip = value++;
  context->last_exception_to_rip = value++;
  context->last_exception_from_rip = value++;
}

namespace {

// Using gtest assertions, compares |expected| to |observed|. This is
// templatized because the CPUContextX86::Fxsave and CPUContextX86_64::Fxsave
// are nearly identical but have different sizes for the members |xmm|,
// |reserved_4|, and |available|.
template <typename FxsaveType>
void ExpectMinidumpContextFxsave(const FxsaveType* expected,
                                 const FxsaveType* observed) {
  EXPECT_EQ(expected->fcw, observed->fcw);
  EXPECT_EQ(expected->fsw, observed->fsw);
  EXPECT_EQ(expected->ftw, observed->ftw);
  EXPECT_EQ(expected->reserved_1, observed->reserved_1);
  EXPECT_EQ(expected->fop, observed->fop);
  EXPECT_EQ(expected->fpu_ip, observed->fpu_ip);
  EXPECT_EQ(expected->fpu_cs, observed->fpu_cs);
  EXPECT_EQ(expected->reserved_2, observed->reserved_2);
  EXPECT_EQ(expected->fpu_dp, observed->fpu_dp);
  EXPECT_EQ(expected->fpu_ds, observed->fpu_ds);
  EXPECT_EQ(expected->reserved_3, observed->reserved_3);
  EXPECT_EQ(expected->mxcsr, observed->mxcsr);
  EXPECT_EQ(expected->mxcsr_mask, observed->mxcsr_mask);
  for (size_t st_mm_index = 0;
       st_mm_index < arraysize(expected->st_mm);
       ++st_mm_index) {
    SCOPED_TRACE(base::StringPrintf("st_mm_index %" PRIuS, st_mm_index));
    EXPECT_EQ(BytesToHexString(expected->st_mm[st_mm_index].st,
                               arraysize(expected->st_mm[st_mm_index].st)),
              BytesToHexString(observed->st_mm[st_mm_index].st,
                               arraysize(observed->st_mm[st_mm_index].st)));
    EXPECT_EQ(
        BytesToHexString(expected->st_mm[st_mm_index].st_reserved,
                         arraysize(expected->st_mm[st_mm_index].st_reserved)),
        BytesToHexString(observed->st_mm[st_mm_index].st_reserved,
                         arraysize(observed->st_mm[st_mm_index].st_reserved)));
  }
  for (size_t xmm_index = 0;
       xmm_index < arraysize(expected->xmm);
       ++xmm_index) {
    EXPECT_EQ(BytesToHexString(expected->xmm[xmm_index],
                               arraysize(expected->xmm[xmm_index])),
              BytesToHexString(observed->xmm[xmm_index],
                               arraysize(observed->xmm[xmm_index])))
        << "xmm_index " << xmm_index;
  }
  EXPECT_EQ(
      BytesToHexString(expected->reserved_4, arraysize(expected->reserved_4)),
      BytesToHexString(observed->reserved_4, arraysize(observed->reserved_4)));
  EXPECT_EQ(
      BytesToHexString(expected->available, arraysize(expected->available)),
      BytesToHexString(observed->available, arraysize(observed->available)));
}

}  // namespace

void ExpectMinidumpContextX86(
    uint32_t expect_seed, const MinidumpContextX86* observed, bool snapshot) {
  MinidumpContextX86 expected;
  InitializeMinidumpContextX86(&expected, expect_seed);

  EXPECT_EQ(expected.context_flags, observed->context_flags);
  EXPECT_EQ(expected.dr0, observed->dr0);
  EXPECT_EQ(expected.dr1, observed->dr1);
  EXPECT_EQ(expected.dr2, observed->dr2);
  EXPECT_EQ(expected.dr3, observed->dr3);
  EXPECT_EQ(expected.dr6, observed->dr6);
  EXPECT_EQ(expected.dr7, observed->dr7);

  EXPECT_EQ(expected.fsave.fcw, observed->fsave.fcw);
  EXPECT_EQ(expected.fsave.fsw, observed->fsave.fsw);
  EXPECT_EQ(expected.fsave.ftw, observed->fsave.ftw);
  EXPECT_EQ(expected.fsave.fpu_ip, observed->fsave.fpu_ip);
  EXPECT_EQ(expected.fsave.fpu_cs, observed->fsave.fpu_cs);
  EXPECT_EQ(expected.fsave.fpu_dp, observed->fsave.fpu_dp);
  EXPECT_EQ(expected.fsave.fpu_ds, observed->fsave.fpu_ds);
  for (size_t index = 0; index < arraysize(expected.fsave.st); ++index) {
    EXPECT_EQ(BytesToHexString(expected.fsave.st[index],
                               arraysize(expected.fsave.st[index])),
              BytesToHexString(observed->fsave.st[index],
                               arraysize(observed->fsave.st[index])))
        << "index " << index;
  }
  if (snapshot) {
    EXPECT_EQ(0u, observed->float_save.spare_0);
  } else {
    EXPECT_EQ(expected.float_save.spare_0, observed->float_save.spare_0);
  }

  EXPECT_EQ(expected.gs, observed->gs);
  EXPECT_EQ(expected.fs, observed->fs);
  EXPECT_EQ(expected.es, observed->es);
  EXPECT_EQ(expected.ds, observed->ds);
  EXPECT_EQ(expected.edi, observed->edi);
  EXPECT_EQ(expected.esi, observed->esi);
  EXPECT_EQ(expected.ebx, observed->ebx);
  EXPECT_EQ(expected.edx, observed->edx);
  EXPECT_EQ(expected.ecx, observed->ecx);
  EXPECT_EQ(expected.eax, observed->eax);
  EXPECT_EQ(expected.ebp, observed->ebp);
  EXPECT_EQ(expected.eip, observed->eip);
  EXPECT_EQ(expected.cs, observed->cs);
  EXPECT_EQ(expected.eflags, observed->eflags);
  EXPECT_EQ(expected.esp, observed->esp);
  EXPECT_EQ(expected.ss, observed->ss);

  ExpectMinidumpContextFxsave(&expected.fxsave, &observed->fxsave);
}

void ExpectMinidumpContextAMD64(
    uint32_t expect_seed, const MinidumpContextAMD64* observed, bool snapshot) {
  MinidumpContextAMD64 expected;
  InitializeMinidumpContextAMD64(&expected, expect_seed);

  EXPECT_EQ(expected.context_flags, observed->context_flags);

  if (snapshot) {
    EXPECT_EQ(0u, observed->p1_home);
    EXPECT_EQ(0u, observed->p2_home);
    EXPECT_EQ(0u, observed->p3_home);
    EXPECT_EQ(0u, observed->p4_home);
    EXPECT_EQ(0u, observed->p5_home);
    EXPECT_EQ(0u, observed->p6_home);
  } else {
    EXPECT_EQ(expected.p1_home, observed->p1_home);
    EXPECT_EQ(expected.p2_home, observed->p2_home);
    EXPECT_EQ(expected.p3_home, observed->p3_home);
    EXPECT_EQ(expected.p4_home, observed->p4_home);
    EXPECT_EQ(expected.p5_home, observed->p5_home);
    EXPECT_EQ(expected.p6_home, observed->p6_home);
  }

  EXPECT_EQ(expected.mx_csr, observed->mx_csr);

  EXPECT_EQ(expected.cs, observed->cs);
  if (snapshot) {
    EXPECT_EQ(0u, observed->ds);
    EXPECT_EQ(0u, observed->es);
  } else {
    EXPECT_EQ(expected.ds, observed->ds);
    EXPECT_EQ(expected.es, observed->es);
  }
  EXPECT_EQ(expected.fs, observed->fs);
  EXPECT_EQ(expected.gs, observed->gs);
  if (snapshot) {
    EXPECT_EQ(0u, observed->ss);
  } else {
    EXPECT_EQ(expected.ss, observed->ss);
  }

  EXPECT_EQ(expected.eflags, observed->eflags);

  EXPECT_EQ(expected.dr0, observed->dr0);
  EXPECT_EQ(expected.dr1, observed->dr1);
  EXPECT_EQ(expected.dr2, observed->dr2);
  EXPECT_EQ(expected.dr3, observed->dr3);
  EXPECT_EQ(expected.dr6, observed->dr6);
  EXPECT_EQ(expected.dr7, observed->dr7);

  EXPECT_EQ(expected.rax, observed->rax);
  EXPECT_EQ(expected.rcx, observed->rcx);
  EXPECT_EQ(expected.rdx, observed->rdx);
  EXPECT_EQ(expected.rbx, observed->rbx);
  EXPECT_EQ(expected.rsp, observed->rsp);
  EXPECT_EQ(expected.rbp, observed->rbp);
  EXPECT_EQ(expected.rsi, observed->rsi);
  EXPECT_EQ(expected.rdi, observed->rdi);
  EXPECT_EQ(expected.r8, observed->r8);
  EXPECT_EQ(expected.r9, observed->r9);
  EXPECT_EQ(expected.r10, observed->r10);
  EXPECT_EQ(expected.r11, observed->r11);
  EXPECT_EQ(expected.r12, observed->r12);
  EXPECT_EQ(expected.r13, observed->r13);
  EXPECT_EQ(expected.r14, observed->r14);
  EXPECT_EQ(expected.r15, observed->r15);
  EXPECT_EQ(expected.rip, observed->rip);

  ExpectMinidumpContextFxsave(&expected.fxsave, &observed->fxsave);

  for (size_t index = 0; index < arraysize(expected.vector_register); ++index) {
    if (snapshot) {
      EXPECT_EQ(0u, observed->vector_register[index].lo) << "index " << index;
      EXPECT_EQ(0u, observed->vector_register[index].hi) << "index " << index;
    } else {
      EXPECT_EQ(expected.vector_register[index].lo,
                observed->vector_register[index].lo) << "index " << index;
      EXPECT_EQ(expected.vector_register[index].hi,
                observed->vector_register[index].hi) << "index " << index;
    }
  }

  if (snapshot) {
    EXPECT_EQ(0u, observed->vector_control);
    EXPECT_EQ(0u, observed->debug_control);
    EXPECT_EQ(0u, observed->last_branch_to_rip);
    EXPECT_EQ(0u, observed->last_branch_from_rip);
    EXPECT_EQ(0u, observed->last_exception_to_rip);
    EXPECT_EQ(0u, observed->last_exception_from_rip);
  } else {
    EXPECT_EQ(expected.vector_control, observed->vector_control);
    EXPECT_EQ(expected.debug_control, observed->debug_control);
    EXPECT_EQ(expected.last_branch_to_rip, observed->last_branch_to_rip);
    EXPECT_EQ(expected.last_branch_from_rip, observed->last_branch_from_rip);
    EXPECT_EQ(expected.last_exception_to_rip, observed->last_exception_to_rip);
    EXPECT_EQ(expected.last_exception_from_rip,
              observed->last_exception_from_rip);
  }
}

}  // namespace test
}  // namespace crashpad
