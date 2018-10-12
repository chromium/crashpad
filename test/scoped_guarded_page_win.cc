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

#include "test/scoped_guarded_page.h"

#include <windows.h>

#include "base/logging.h"
#include "base/process/process_metrics.h"

namespace crashpad {
namespace test {

ScopedGuardedPage::ScopedGuardedPage() {
  ptr_ = VirtualAlloc(
      nullptr, base::GetPageSize() * 2, MEM_RESERVE, PAGE_NOACCESS);
  if (ptr_ == nullptr) {
    CHECK(false) << "VirtualAlloc region";
  }

  void* ret =
      VirtualAlloc(ptr_, base::GetPageSize(), MEM_COMMIT, PAGE_READWRITE);
  if (ret == nullptr) {
    CHECK(false) << "VirtualAlloc page";
  }
}

ScopedGuardedPage::~ScopedGuardedPage() {
  VirtualFree(ptr_, 0, MEM_RELEASE);
}

}  // namespace test
}  // namespace crashpad
