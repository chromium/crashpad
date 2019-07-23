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

#include "snapshot/cpu_context.h"

#include "base/logging.h"

namespace crashpad {

uint64_t CPUContext::InstructionPointer() const {
  switch (architecture) {
    case kCPUArchitectureX86:
      return x86->eip;
    case kCPUArchitectureX86_64:
      return x86_64->rip;
    case kCPUArchitectureARM:
      return arm->pc;
    case kCPUArchitectureARM64:
      return arm64->pc;
    default:
      NOTREACHED();
      return ~0ull;
  }
}

uint64_t CPUContext::StackPointer() const {
  switch (architecture) {
    case kCPUArchitectureX86:
      return x86->esp;
    case kCPUArchitectureX86_64:
      return x86_64->rsp;
    case kCPUArchitectureARM:
      return arm->sp;
    case kCPUArchitectureARM64:
      return arm64->sp;
    default:
      NOTREACHED();
      return ~0ull;
  }
}

bool CPUContext::Is64Bit() const {
  switch (architecture) {
    case kCPUArchitectureX86_64:
    case kCPUArchitectureARM64:
    case kCPUArchitectureMIPS64EL:
      return true;
    case kCPUArchitectureX86:
    case kCPUArchitectureARM:
    case kCPUArchitectureMIPSEL:
      return false;
    default:
      NOTREACHED();
      return false;
  }
}

}  // namespace crashpad
