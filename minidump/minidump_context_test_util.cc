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

#include "minidump/minidump_context_test_util.h"

#include "base/basictypes.h"
#include "gtest/gtest.h"

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

  context->dr0 = value++;
  context->dr1 = value++;
  context->dr2 = value++;
  context->dr3 = value++;
  context->dr6 = value++;
  context->dr7 = value++;
  context->float_save.control_word = value++;
  context->float_save.status_word = value++;
  context->float_save.tag_word = value++;
  context->float_save.error_offset = value++;
  context->float_save.error_selector = value++;
  context->float_save.data_offset = value++;
  context->float_save.data_selector = value++;
  for (size_t index = 0;
       index < arraysize(context->float_save.register_area);
       ++index) {
    context->float_save.register_area[index] = value++;
  }
  context->float_save.spare_0 = value++;
  context->gs = value++;
  context->fs = value++;
  context->es = value++;
  context->ds = value++;
  context->edi = value++;
  context->esi = value++;
  context->ebx = value++;
  context->edx = value++;
  context->ecx = value++;
  context->eax = value++;
  context->ebp = value++;
  context->eip = value++;
  context->cs = value++;
  context->eflags = value++;
  context->esp = value++;
  context->ss = value++;
  for (size_t index = 0; index < arraysize(context->extended_registers);
       ++index) {
    context->extended_registers[index] = value++;
  }
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

  context->p1_home = value++;
  context->p2_home = value++;
  context->p3_home = value++;
  context->p4_home = value++;
  context->p5_home = value++;
  context->p6_home = value++;
  context->mx_csr = value++;
  context->cs = value++;
  context->ds = value++;
  context->es = value++;
  context->fs = value++;
  context->gs = value++;
  context->ss = value++;
  context->eflags = value++;
  context->dr0 = value++;
  context->dr1 = value++;
  context->dr2 = value++;
  context->dr3 = value++;
  context->dr6 = value++;
  context->dr7 = value++;
  context->rax = value++;
  context->rcx = value++;
  context->rdx = value++;
  context->rbx = value++;
  context->rsp = value++;
  context->rbp = value++;
  context->rsi = value++;
  context->rdi = value++;
  context->r8 = value++;
  context->r9 = value++;
  context->r10 = value++;
  context->r11 = value++;
  context->r12 = value++;
  context->r13 = value++;
  context->r14 = value++;
  context->r15 = value++;
  context->rip = value++;
  context->float_save.control_word = value++;
  context->float_save.status_word = value++;
  context->float_save.tag_word = value++;
  context->float_save.reserved_1 = value++;
  context->float_save.error_opcode = value++;
  context->float_save.error_offset = value++;
  context->float_save.error_selector = value++;
  context->float_save.reserved_2 = value++;
  context->float_save.data_offset = value++;
  context->float_save.data_selector = value++;
  context->float_save.reserved_3 = value++;
  context->float_save.mx_csr = value++;
  context->float_save.mx_csr_mask = value++;
  for (size_t index = 0;
       index < arraysize(context->float_save.float_registers);
       ++index) {
    context->float_save.float_registers[index].lo = value++;
    context->float_save.float_registers[index].hi = value++;
  }
  for (size_t index = 0;
       index < arraysize(context->float_save.xmm_registers);
       ++index) {
    context->float_save.xmm_registers[index].lo = value++;
    context->float_save.xmm_registers[index].hi = value++;
  }
  for (size_t index = 0;
       index < arraysize(context->float_save.reserved_4);
       ++index) {
    context->float_save.reserved_4[index] = value++;
  }
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

void ExpectMinidumpContextX86(uint32_t expect_seed,
                              const MinidumpContextX86* observed) {
  MinidumpContextX86 expected;
  InitializeMinidumpContextX86(&expected, expect_seed);

  EXPECT_EQ(expected.context_flags, observed->context_flags);
  EXPECT_EQ(expected.dr0, observed->dr0);
  EXPECT_EQ(expected.dr1, observed->dr1);
  EXPECT_EQ(expected.dr2, observed->dr2);
  EXPECT_EQ(expected.dr3, observed->dr3);
  EXPECT_EQ(expected.dr6, observed->dr6);
  EXPECT_EQ(expected.dr7, observed->dr7);
  EXPECT_EQ(expected.float_save.control_word,
            observed->float_save.control_word);
  EXPECT_EQ(expected.float_save.status_word, observed->float_save.status_word);
  EXPECT_EQ(expected.float_save.tag_word, observed->float_save.tag_word);
  EXPECT_EQ(expected.float_save.error_offset,
            observed->float_save.error_offset);
  EXPECT_EQ(expected.float_save.error_selector,
            observed->float_save.error_selector);
  EXPECT_EQ(expected.float_save.data_offset, observed->float_save.data_offset);
  EXPECT_EQ(expected.float_save.data_selector,
            observed->float_save.data_selector);
  for (size_t index = 0;
       index < arraysize(expected.float_save.register_area);
       ++index) {
    EXPECT_EQ(expected.float_save.register_area[index],
              observed->float_save.register_area[index]);
  }
  EXPECT_EQ(expected.float_save.spare_0, observed->float_save.spare_0);
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
  for (size_t index = 0;
       index < arraysize(expected.extended_registers);
       ++index) {
    EXPECT_EQ(expected.extended_registers[index],
              observed->extended_registers[index]);
  }
}

void ExpectMinidumpContextAMD64(uint32_t expect_seed,
                                const MinidumpContextAMD64* observed) {
  MinidumpContextAMD64 expected;
  InitializeMinidumpContextAMD64(&expected, expect_seed);

  EXPECT_EQ(expected.context_flags, observed->context_flags);
  EXPECT_EQ(expected.p1_home, observed->p1_home);
  EXPECT_EQ(expected.p2_home, observed->p2_home);
  EXPECT_EQ(expected.p3_home, observed->p3_home);
  EXPECT_EQ(expected.p4_home, observed->p4_home);
  EXPECT_EQ(expected.p5_home, observed->p5_home);
  EXPECT_EQ(expected.p6_home, observed->p6_home);
  EXPECT_EQ(expected.mx_csr, observed->mx_csr);
  EXPECT_EQ(expected.cs, observed->cs);
  EXPECT_EQ(expected.ds, observed->ds);
  EXPECT_EQ(expected.es, observed->es);
  EXPECT_EQ(expected.fs, observed->fs);
  EXPECT_EQ(expected.gs, observed->gs);
  EXPECT_EQ(expected.ss, observed->ss);
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
  EXPECT_EQ(expected.float_save.control_word,
            observed->float_save.control_word);
  EXPECT_EQ(expected.float_save.status_word, observed->float_save.status_word);
  EXPECT_EQ(expected.float_save.tag_word, observed->float_save.tag_word);
  EXPECT_EQ(expected.float_save.reserved_1, observed->float_save.reserved_1);
  EXPECT_EQ(expected.float_save.error_opcode,
            observed->float_save.error_opcode);
  EXPECT_EQ(expected.float_save.error_offset,
            observed->float_save.error_offset);
  EXPECT_EQ(expected.float_save.error_selector,
            observed->float_save.error_selector);
  EXPECT_EQ(expected.float_save.reserved_2, observed->float_save.reserved_2);
  EXPECT_EQ(expected.float_save.data_offset, observed->float_save.data_offset);
  EXPECT_EQ(expected.float_save.data_selector,
            observed->float_save.data_selector);
  EXPECT_EQ(expected.float_save.reserved_3, observed->float_save.reserved_3);
  EXPECT_EQ(expected.float_save.mx_csr, observed->float_save.mx_csr);
  EXPECT_EQ(expected.float_save.mx_csr_mask, observed->float_save.mx_csr_mask);
  for (size_t index = 0;
       index < arraysize(expected.float_save.float_registers);
       ++index) {
    EXPECT_EQ(expected.float_save.float_registers[index].lo,
              observed->float_save.float_registers[index].lo);
    EXPECT_EQ(expected.float_save.float_registers[index].hi,
              observed->float_save.float_registers[index].hi);
  }
  for (size_t index = 0;
       index < arraysize(expected.float_save.xmm_registers);
       ++index) {
    EXPECT_EQ(expected.float_save.xmm_registers[index].lo,
              observed->float_save.xmm_registers[index].lo);
    EXPECT_EQ(expected.float_save.xmm_registers[index].hi,
              observed->float_save.xmm_registers[index].hi);
  }
  for (size_t index = 0;
       index < arraysize(expected.float_save.reserved_4);
       ++index) {
    EXPECT_EQ(expected.float_save.reserved_4[index],
              observed->float_save.reserved_4[index]);
  }
  for (size_t index = 0; index < arraysize(expected.vector_register); ++index) {
    EXPECT_EQ(expected.vector_register[index].lo,
              observed->vector_register[index].lo);
    EXPECT_EQ(expected.vector_register[index].hi,
              observed->vector_register[index].hi);
  }
  EXPECT_EQ(expected.vector_control, observed->vector_control);
  EXPECT_EQ(expected.debug_control, observed->debug_control);
  EXPECT_EQ(expected.last_branch_to_rip, observed->last_branch_to_rip);
  EXPECT_EQ(expected.last_branch_from_rip, observed->last_branch_from_rip);
  EXPECT_EQ(expected.last_exception_to_rip, observed->last_exception_to_rip);
  EXPECT_EQ(expected.last_exception_from_rip,
            observed->last_exception_from_rip);
}

}  // namespace test
}  // namespace crashpad
