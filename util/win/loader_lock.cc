// Copyright 2019 The Crashpad Authors. All rights reserved.
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

#include "util/win/loader_lock.h"

#include <intrin.h>
#include <stddef.h>
#include <windows.h>

#include "build/build_config.h"
#include "util/win/process_structs.h"

namespace crashpad {

namespace {

#if defined(ARCH_CPU_X86)
using NativeTraits = process_types::internal::Traits32;
DWORD_PTR ReadBase(size_t offset) {
  return __readfsdword(offset);
}
#elif defined(ARCH_CPU_X86_64)
using NativeTraits = process_types::internal::Traits64;
DWORD_PTR ReadBase(size_t offset) {
  return __readgsqword(static_cast<unsigned long>(offset));
}
#else
// TODO: Implement ARM64 support.
using NativeTraits = process_types::internal::Traits64;
DWORD_PTR ReadBase(size_t offset) {
  return 0;
}
#endif  // defined(ARCH_CPU_X86)

using NT_TIB = process_types::NT_TIB<NativeTraits>;
using TEB = process_types::TEB<NativeTraits>;
using PEB = process_types::PEB<NativeTraits>;

PEB* GetPeb() {
  auto* teb = reinterpret_cast<TEB*>(ReadBase(offsetof(NT_TIB, Self)));
  if (!teb)
    return nullptr;
  return reinterpret_cast<PEB*>(teb->ProcessEnvironmentBlock);
}

}  // namespace

bool IsThreadInLoaderLock() {
  auto* peb = GetPeb();
  if (!peb)
    return false;

  auto* loader_lock = reinterpret_cast<PCRITICAL_SECTION>(peb->LoaderLock);
  HANDLE tid =
      reinterpret_cast<HANDLE>(static_cast<DWORD_PTR>(::GetCurrentThreadId()));
  return loader_lock->LockCount && loader_lock->OwningThread == tid;
}

}  // namespace crashpad
