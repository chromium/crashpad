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

// namespace crashpad {
// void CaptureContext(ucontext_t* context);
// }  // namespace crashpad
#define CAPTURECONTEXT_SYMBOL _ZN8crashpad14CaptureContextEP8ucontext

  .text
  .globl CAPTURECONTEXT_SYMBOL
  .align 4, 0x90
CAPTURECONTEXT_SYMBOL:

#if defined(__i386__)

  .cfi_startproc

  pushl %ebp
  .cfi_def_cfa_offset 8
  .cfi_offset %ebp, -8
  movl %esp, %ebp
  .cfi_def_cfa_register %ebp

  // Note that 16-byte stack alignment is not maintained because this function
  // does not call out to any other.

  // pushfl first, because some instructions (but probably none used here)
  // affect %eflags. %eflags will be in -4(%ebp).
  pushfl

  // Save the original value of %eax, and use %eax to hold the ucontext_t*
  // argument. The original value of %eax will be in -8(%ebp).
  pushl %eax
  movl 8(%ebp), %eax

  // Save the original value of %ecx, and use %ecx as a scratch register.
  pushl %ecx

  // The segment registers are 16 bits wide, but mcontext_t declares them
  // as unsigned 32-bit values, so zero the top half.
  xorl %ecx, %ecx
  movw %gs, %cx
  movl %ecx, 0x20(%eax)  // context->uc_mcontext.xgs
  movw %fs, %cx
  movl %ecx, 0x24(%eax)  // context->uc_mcontext.xfs
  movw %es, %cx
  movl %ecx, 0x28(%eax)  // context->uc_mcontext.xes
  movw %ds, %cx
  movl %ecx, 0x2c(%eax)  // context->uc_mcontext.xds

  // General-purpose registers whose values haven’t changed can be captured
  // directly.
  movl %edi, 0x30(%eax)  // context->uc_mcontext.edi
  movl %esi, 0x34(%eax)  // context->uc_mcontext.esi

  // The original %ebp was saved on the stack in this function’s prologue.
  movl (%ebp), %ecx
  movl %ecx, 0x38(%eax)  // context->uc_mcontext.ebp

  // %esp was saved in %ebp in this function’s prologue, but the caller’s %esp
  // is 8 more than this value: 4 for the original %ebp saved on the stack in
  // this function’s prologue, and 4 for the return address saved on the stack
  // by the call instruction that reached this function.
  leal 8(%ebp), %ecx
  movl %ecx, 0x3c(%eax)  // context->uc_mcontext.esp

  movl %ebx, 0x40(%eax)  // context->uc_mcontext.ebx
  movl %edx, 0x44(%eax)  // context->uc_mcontext.edx

  // The original %ecx was saved on the stack above.
  movl -12(%ebp), %ecx
  movl %ecx, 0x48(%eax)  // context->uc_mcontext.ecx

  // The original %eax was saved on the stack above.
  movl -8(%ebp), %ecx
  movl %ecx, 0x50(%eax)  // context->uc_mcontext.eax

  // trapno and err are unused.
  xorl %ecx, %ecx
  movl %ecx, 0x54(%eax)  // context->uc_mcontext.trapno
  movl %ecx, 0x58(%eax)  // context->uc_mcontext.err

  // %eip can’t be accessed directly, but the return address saved on the stack
  // by the call instruction that reached this function can be used.
  movl 4(%ebp), %ecx
  movl %ecx, 0x5c(%eax)  // context->uc_mcontext.eip

  xorl %ecx, %ecx
  movw %cs, %cx
  movl %ecx, 0x60(%eax)  // context->uc_mcontext.xcs

  // The original %eflags was saved on the stack above.
  movl -4(%ebp), %ecx
  movl %ecx, 0x64(%eax)  // context->uc_mcontext.eflags

  // uesp is unused
  xorl %ecx, %ecx
  movl %ecx, 0x68(%eax)  // context->uc_mcontext.uesp

  movw %ss, %cx
  movl %ecx, 0x6c(%eax)  // context->uc_mcontext.xss

  // TODO(jperaza): zero unsued ucontext fields and collect fpregs

  // Clean up by restoring clobbered registers, even those considered volatile
  // by the ABI, so that the captured context represents the state at this
  // function’s exit.
  popl %ecx
  popl %eax
  popfl

  popl %ebp

  ret

  .cfi_endproc

#elif defined(__x86_64__)

  .cfi_startproc

  pushq %rbp
  .cfi_def_cfa_offset 16
  .cfi_offset %rbp, -16
  movq %rsp, %rbp
  .cfi_def_cfa_register %rbp

  // Note that 16-byte stack alignment is not maintained because this function
  // does not call out to any other.

  // pushfq first, because some instructions (but probably none used here)
  // affect %rflags. %rflags will be in -8(%rbp).
  pushfq

  // General-purpose registers whose values haven’t changed can be captured
  // directly.

  // GPRS are stored in an mcontext at offset 0x28
  movq %r8, 0x28(%rdi)  // context->uc_mcontext.r8
  movq %r9, 0x30(%rdi)  // context->uc_mcontext.r9
  movq %r10, 0x38(%rdi)  // context->uc_mcontext.r10
  movq %r11, 0x40(%rdi)  // context->uc_mcontext.r11
  movq %r12, 0x48(%rdi)  // context->uc_mcontext.r12
  movq %r13, 0x50(%rdi)  // context->uc_mcontext.r13
  movq %r14, 0x58(%rdi)  // context->uc_mcontext.r14
  movq %r15, 0x60(%rdi)  // context->uc_mcontext.r15

  // Because of the calling convention, there’s no way to recover the value of
  // the caller’s %rdi as it existed prior to calling this function. This
  // function captures a snapshot of the register state at its return, which
  // involves %rdi containing a pointer to its first argument. Callers that
  // require the value of %rdi prior to calling this function should obtain it
  // separately. For example:
  //   uint64_t rdi;
  //   asm("movq %%rdi, %0" : "=m"(rdi));
  movq %rdi, 0x68(%rdi)  // context->uc_mcontext.rdi

  movq %rsi, 0x70(%rdi)  // context->uc_mcontext.rsi

  // Now that the original value of %r8 has been saved, it can be repurposed to
  // hold other registers’ values.

  // The original %rbp was saved on the stack in this function’s prologue.
  movq (%rbp), %r8
  movq %r8, 0x78(%rdi)  // context->uc_mcontext.rbp

  movq %rbx, 0x80(%rdi)  // context->uc_mcontext.rbx
  movq %rdx, 0x88(%rdi)  // context->uc_mcontext.rdx
  movq %rax, 0x90(%rdi)  // context->uc_mcontext.rax
  movq %rcx, 0x98(%rdi)  // context->uc_mcontext.rcx

  // %rsp was saved in %rbp in this function’s prologue, but the caller’s %rsp
  // is 16 more than this value: 8 for the original %rbp saved on the stack in
  // this function’s prologue, and 8 for the return address saved on the stack
  // by the call instruction that reached this function.
  leaq 16(%rbp), %r8
  movq %r8, 0xa0(%rdi)  // context->uc_mcontext.rsp

  // %rip can’t be accessed directly, but the return address saved on the stack
  // by the call instruction that reached this function can be used.
  movq 8(%rbp), %r8
  movq %r8, 0xa8(%rdi)  // context->uc_mcontext.rip

  // The original %rflags was saved on the stack above.
  movq -8(%rbp), %r8
  movq %r8, 0xb0(%rdi)  // context->uc_mcontext.eflags

  movw %cs, 0xb8(%rdi)  // context->uc_mcontext.cs
  movw %gs, 0xba(%rdi)  // context->uc_mcontext.gs
  movw %fs, 0xbc(%rdi)  // context->uc_mcontext.fs

  xorw %ax, %ax
  movw %ax, 0xbe(%rdi)  // context->uc_mcontext.padding

  // Clean up by restoring clobbered registers, even those considered volatile
  // by the ABI, so that the captured context represents the state at this
  // function’s exit.
  movq 0x90(%rdi), %rax
  movq 0x28(%rdi), %r8

  // TODO(jperaza): zero out unused portions of the context and save fpregs.

  popfq

  popq %rbp

  ret

  .cfi_endproc

#elif defined(__arm__)

  .cfi_startproc

  // mcontext starts at offset 0x20

  // original r0 can't be recovered
  str r0, [r0, #0x2c]  // context->uc_mcontext.regs[0]
  str r1, [r0, #0x30]  // context->uc_mcontext.regs[1]
  str r2, [r0, #0x34]  // context->uc_mcontext.regs[2]
  str r3, [r0, #0x38]  // context->uc_mcontext.regs[3]
  str r4, [r0, #0x3c]  // context->uc_mcontext.regs[4]
  str r5, [r0, #0x40]  // context->uc_mcontext.regs[5]
  str r6, [r0, #0x44]  // context->uc_mcontext.regs[6]
  str r7, [r0, #0x48]  // context->uc_mcontext.regs[7]
  str r8, [r0, #0x4c]  // context->uc_mcontext.regs[8]
  str r9, [r0, #0x50]  // context->uc_mcontext.regs[9]
  str r10, [r0, #0x54]  // context->uc_mcontext.regs[10]
  str FP, [r0, #0x58]  // context->uc_mcontext.fp
  str IP, [r0, #0x5c]  // context->uc_mcontext.ip
  str SP, [r0, #0x60]  // context->uc_mcontext.sp

  // TODO original LR possible to determine?
  // If caller used bx, this will be the original LR?
  str LR, [r0, #0x64]  // context->uc_mcontext.lr

  // The link register holds the return address for this function.
  str LR, [r0, #0x68]  // context->uc_mcontext.pc

  // Use r1 to save CPSR, then restore it from the saved context
  mrs r1, APSR
  str r1, [r0, #0x6c]  // context->uc_mcontext.cpsr
  ldr r1, [r0, #0x30]

  // TODO(jperaza): zero out unused context space and save fpregs.

  // TODO(jperaza): Docs say bx only works on thumb capable processors.
  // Is that a problem?
  bx LR

  .cfi_endproc

#elif defined(__aarch64__)

  .cfi_startproc

  // mcontext starts at offset 0xb0
  // fault_address = 0

  // original r0 can't be recovered
  str x0, [x0, #0xb8]  // context->uc_mcontext.regs[0]
  str x1, [x0, #0xc0]  // context->uc_mcontext.regs[0]
  str x2, [x0, #0xc8]  // context->uc_mcontext.regs[0]
  str x3, [x0, #0xd0]  // context->uc_mcontext.regs[0]
  str x4, [x0, #0xd8]  // context->uc_mcontext.regs[0]
  str x5, [x0, #0xe0]  // context->uc_mcontext.regs[0]
  str x6, [x0, #0xe8]  // context->uc_mcontext.regs[0]
  str x7, [x0, #0xf0]  // context->uc_mcontext.regs[0]
  str x8, [x0, #0xf8]  // context->uc_mcontext.regs[0]
  str x9, [x0, #0x100]  // context->uc_mcontext.regs[0]
  str x10, [x0, #0x108]  // context->uc_mcontext.regs[0]
  str x11, [x0, #0x110]  // context->uc_mcontext.regs[0]
  str x12, [x0, #0x118]  // context->uc_mcontext.regs[0]
  str x13, [x0, #0x120]  // context->uc_mcontext.regs[0]
  str x14, [x0, #0x128]  // context->uc_mcontext.regs[0]
  str x15, [x0, #0x130]  // context->uc_mcontext.regs[0]
  str x16, [x0, #0x138]  // context->uc_mcontext.regs[0]
  str x17, [x0, #0x140]  // context->uc_mcontext.regs[0]
  str x18, [x0, #0x148]  // context->uc_mcontext.regs[0]
  str x19, [x0, #0x150]  // context->uc_mcontext.regs[0]
  str x20, [x0, #0x158]  // context->uc_mcontext.regs[0]
  str x21, [x0, #0x160]  // context->uc_mcontext.regs[0]
  str x22, [x0, #0x168]  // context->uc_mcontext.regs[0]
  str x23, [x0, #0x170]  // context->uc_mcontext.regs[0]
  str x24, [x0, #0x178]  // context->uc_mcontext.regs[0]
  str x25, [x0, #0x180]  // context->uc_mcontext.regs[0]
  str x26, [x0, #0x188]  // context->uc_mcontext.regs[0]
  str x27, [x0, #0x190]  // context->uc_mcontext.regs[0]
  str x28, [x0, #0x198]  // context->uc_mcontext.regs[0]
  str x29, [x0, #0x1a0]  // context->uc_mcontext.regs[0]
  str x30, [x0, #0x1a8]  // context->uc_mcontext.regs[0]

  str x31, [x0, #0x1b0] // context->uc_mcontext.sp

  // The link register holds the return address for this function.
  str LR, [x0, #0x1b8]  // context->uc_mcontext.pc

  // Use x1 to save CPSR, then restore it from the saved context
  mrs x1, NZCV
  str x1, [x0, #0x1c0]  // context->uc_mcontext.pstate
  ldr x1, [x0, #0xc0]

  // TODO(jperaza): zero unused space and save fpregs

  ret

  .cfi_endproc

#endif  // __i386__
