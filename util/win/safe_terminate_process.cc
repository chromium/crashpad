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

#include "util/win/safe_terminate_process.h"

#if defined(ARCH_CPU_X86)

namespace crashpad {

// This function is written in assembler source because it’s important for it to
// not be inlined, for it to allocate a stack frame, and most critically, for it
// to not trust esp on return from TerminateProcess(). __declspec(naked)
// conveniently prevents inlining and allows control of stack layout.
__declspec(naked) bool SafeTerminateProcess(HANDLE process, UINT exit_code) {
  __asm {
    push ebp
    mov ebp, esp

    push [ebp+12]
    push [ebp+8]
    call TerminateProcess

    // Convert from BOOL to bool.
    test eax, eax
    setne al

    // TerminateProcess() is supposed to be stdcall (callee clean-up), and esp
    // and ebp are expected to already be equal. But if it’s been patched badly
    // by something that’s cdecl (caller clean-up), this next move will get
    // things back on track.
    mov esp, ebp
    pop ebp

    ret
  }
}

}  // namespace crashpad

#endif  // defined(ARCH_CPU_X86)
