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

#include "snapshot/fuchsia/module_snapshot_fuchsia.h"

namespace crashpad {
namespace internal {

ModuleSnapshotFuchsia::ModuleSnapshotFuchsia() = default;

ModuleSnapshotFuchsia::~ModuleSnapshotFuchsia() = default;

bool ModuleSnapshotFuchsia::Initialize(ProcessReader* process_reader,
                  const ProcessReader::Module& process_reader_module) {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);

  process_reader_ = process_reader;
  name_ = process_reader_module.name;
  elf_image_reader_ = process_reader_module.reader;
  if (!elf_image_reader_) {
    return false;
  }

  INITIALIZATION_STATE_SET_VALID(initialized_);
  return true;
}

void ModuleSnapshotFuchsia::GetCrashpadOptions(
    CrashpadInfoClientOptions* options) {
}

}  // namespace internal
}  // namespace crashpad
