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

#ifndef CRASHPAD_TEST_SCOPED_UNSET_CRASHPAD_INFO_H_
#define CRASHPAD_TEST_SCOPED_UNSET_CRASHPAD_INFO_H_

#include "base/macros.h"
#include "client/crashpad_info.h"

namespace crashpad {
namespace test {

//! \brief Resets members of CrashpadInfo to default state when destroyed.
class ScopedUnsetCrashpadInfo {
 public:
  explicit ScopedUnsetCrashpadInfo(CrashpadInfo* crashpad_info)
      : crashpad_info_(crashpad_info) {}

  ~ScopedUnsetCrashpadInfo() {
    crashpad_info_->set_crashpad_handler_behavior(TriState::kUnset);
    crashpad_info_->set_system_crash_reporter_forwarding(TriState::kUnset);
    crashpad_info_->set_gather_indirectly_referenced_memory(TriState::kUnset,
                                                            0);
    crashpad_info_->set_extra_memory_ranges(nullptr);
    crashpad_info_->set_simple_annotations(nullptr);
    crashpad_info_->set_annotations_list(nullptr);
  }

 private:
  CrashpadInfo* crashpad_info_;

  DISALLOW_COPY_AND_ASSIGN(ScopedUnsetCrashpadInfo);
};

}  // namespace test
}  // namespace crashpad

#endif  // CRASHPAD_TEST_SCOPED_UNSET_CRASHPAD_INFO_H_
