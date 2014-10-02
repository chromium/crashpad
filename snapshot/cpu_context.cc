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
    default:
      NOTREACHED();
      return -1;
  }
}

}  // namespace crashpad
