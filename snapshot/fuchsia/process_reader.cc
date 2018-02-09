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

#include "snapshot/fuchsia/process_reader.h"

namespace crashpad {

ProcessReader::Module::Module() = default;

ProcessReader::Module::~Module() = default;

ProcessReader::ProcessReader() = default;

ProcessReader::~ProcessReader() = default;

bool ProcessReader::Initialize(zx_handle_t process) {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);

  process_ = process;

  process_memory_.reset(new ProcessMemoryFuchsia());
  process_memory_->Initialize(process_);

  INITIALIZATION_STATE_SET_VALID(initialized_);
  return true;
}

const std::vector<ProcessReader::Module>& ProcessReader::Modules() {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

  if (!initialized_modules_) {
    InitializeModules();
  }

  return modules_;
}

void ProcessReader::InitializeModules() {
  DCHECK(!initialized_modules_);
  DCHECK(modules_.empty());

  initialized_modules_ = true;

  // TODO actually load modules here

  Module module;
  module.timestamp = 0;
  //process_memory_->ReadCString(...);

  std::unique_ptr<ElfImageReader> reader(new ElfImageReader());
  //reader->Initialize(process_memory_range, module_address);
  module.reader = reader.get();
  module_readers_.push_back(std::move(reader));
  modules_.push_back(module);
}

}  // namespace crashpad
