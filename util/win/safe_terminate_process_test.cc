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

#include <string.h>

#include <memory>

#include "base/logging.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "gtest/gtest.h"
#include "test/errors.h"
#include "util/win/scoped_handle.h"

namespace crashpad {
namespace test {
namespace {

// Patches executable code, saving a copy of the original code so that it can be
// restored on destruction.
class ScopedExecutablePatch {
 public:
  ScopedExecutablePatch(void* target, const void* source, size_t size)
      : original_(new uint8_t[size]), target_(target), size_(size) {
    memcpy(original_.get(), target_, size_);

    ScopedVirtualProtectRWX protect_rwx(target_, size_);
    memcpy(target_, source, size_);
  }

  ~ScopedExecutablePatch() {
    ScopedVirtualProtectRWX protect_rwx(target_, size_);
    memcpy(target_, original_.get(), size_);
  }

 private:
  // Sets the protection on (address, size) to PAGE_EXECUTE_READWRITE by calling
  // VirtualProtect(), and restores the original protection on destruction. Note
  // that the region may span multiple pages, but the first page’s original
  // protection will be applied to the entire region on destruction. This
  // shouldn’t be a problem in practice for patching functions.
  class ScopedVirtualProtectRWX {
   public:
    // If either the constructor or destructor fails, PCHECK() to terminate
    // immediately, because the process will be in a weird and untrustworthy
    // state, and gtest error handling isn’t worthwhile at that point.

    ScopedVirtualProtectRWX(void* address, size_t size)
        : address_(address), size_(size) {
      PCHECK(VirtualProtect(
          address_, size_, PAGE_EXECUTE_READWRITE, &old_protect_))
          << "VirtualProtect";
    }

    ~ScopedVirtualProtectRWX() {
      DWORD last_protect_;
      PCHECK(VirtualProtect(address_, size_, old_protect_, &last_protect_))
          << "VirtualProtect";
    }

   private:
    void* address_;
    size_t size_;
    DWORD old_protect_;

    DISALLOW_COPY_AND_ASSIGN(ScopedVirtualProtectRWX);
  };

  std::unique_ptr<uint8_t[]> original_;
  void* target_;
  size_t size_;

  DISALLOW_COPY_AND_ASSIGN(ScopedExecutablePatch);
};

TEST(SafeTerminateProcess, SafeTerminateProcess) {
  // Notably, don’t create process with PROCESS_TERMINATE access or with the
  // DUPLICATE_SAME_ACCESS option. The SafeTerminateProcess() calls that follow
  // operate on a duplicate of the current process’ process handle, and they’re
  // supposed to fail, not terminate this process.
  HANDLE process;
  ASSERT_TRUE(DuplicateHandle(GetCurrentProcess(),
                              GetCurrentProcess(),
                              GetCurrentProcess(),
                              &process,
                              PROCESS_QUERY_INFORMATION,
                              false,
                              0))
      << ErrorMessage("DuplicateHandle");
  ScopedKernelHANDLE process_owner(process);

  // Make sure that TerminateProcess() works as a baseline.
  SetLastError(ERROR_SUCCESS);
  EXPECT_FALSE(TerminateProcess(process, 0));
  EXPECT_EQ(GetLastError(), ERROR_ACCESS_DENIED);

  // Make sure that SafeTerminateProcess() works, calling through to
  // TerminateProcess() properly.
  SetLastError(ERROR_SUCCESS);
  EXPECT_FALSE(SafeTerminateProcess(process, 0));
  EXPECT_EQ(GetLastError(), ERROR_ACCESS_DENIED);

#if defined(ARCH_CPU_X86)
  {
    // Patch TerminateProcess() badly. This turns it into a no-op that returns 0
    // without cleaning up arguments from the stack, as a stdcall function is
    // expected to do.
    //
    // This simulates the unexpected cdecl-patched TerminateProcess() as seen at
    // https://crashpad.chromium.org/bug/179.
    const uint8_t patch[] = {
        0x31, 0xc0,  // xor eax, eax
        0xc3,  // ret
    };

    void* target = reinterpret_cast<void*>(TerminateProcess);
    ScopedExecutablePatch executable_patch(target, patch, arraysize(patch));

    // Make sure that SafeTerminateProcess() can be called. Since it’s been
    // patched with a no-op stub, GetLastError() shouldn’t be modified.
    SetLastError(ERROR_SUCCESS);
    EXPECT_FALSE(SafeTerminateProcess(process, 0));
    EXPECT_EQ(GetLastError(), ERROR_SUCCESS);
  }

  // Now that the real TerminateProcess() has been restored, verify that it
  // still works properly.
  SetLastError(ERROR_SUCCESS);
  EXPECT_FALSE(SafeTerminateProcess(process, 0));
  EXPECT_EQ(GetLastError(), ERROR_ACCESS_DENIED);
#endif  // ARCH_CPU_X86
}

}  // namespace
}  // namespace test
}  // namespace crashpad
