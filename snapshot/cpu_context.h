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

#ifndef CRASHPAD_SNAPSHOT_SNAPSHOT_CPU_CONTEXT_H_
#define CRASHPAD_SNAPSHOT_SNAPSHOT_CPU_CONTEXT_H_

#include <stdint.h>

#include "snapshot/cpu_architecture.h"
#include "util/misc/cpu_context.h"

namespace crashpad {

//! \brief A context structure capable of carrying the context of any supported
//!     CPU architecture.
struct CPUContext {
  //! \brief Returns the instruction pointer value from the context structure.
  //!
  //! This is a CPU architecture-independent method that is capable of
  //! recovering the instruction pointer from any supported CPU architecture’s
  //! context structure.
  uint64_t InstructionPointer() const;

  //! \brief Returns the stack pointer value from the context structure.
  //!
  //! This is a CPU architecture-independent method that is capable of
  //! recovering the stack pointer from any supported CPU architecture’s
  //! context structure.
  uint64_t StackPointer() const;

  //! \brief Returns `true` if this context is for a 64-bit architecture.
  bool Is64Bit() const;

  //! \brief The CPU architecture of a context structure. This field controls
  //!     the expression of the union.
  CPUArchitecture architecture;
  union {
    CPUContextX86* x86;
    CPUContextX86_64* x86_64;
    CPUContextARM* arm;
    CPUContextARM64* arm64;
    CPUContextMIPS* mipsel;
    CPUContextMIPS64* mips64;
  };
};

}  // namespace crashpad

#endif  // CRASHPAD_SNAPSHOT_SNAPSHOT_CPU_CONTEXT_H_
