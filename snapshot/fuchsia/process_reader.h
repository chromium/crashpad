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

#ifndef CRASHPAD_SNAPSHOT_FUCHSIA_PROCESS_READER_H_
#define CRASHPAD_SNAPSHOT_FUCHSIA_PROCESS_READER_H_

#include <vector>

#include "base/macros.h"
#include "build/build_config.h"
#include "util/misc/initialization_state_dcheck.h"

namespace crashpad {

//! \brief Accesses information about another process, identified by a Fuchsia
//!     process.
class ProcessReader {
 public:
  //! \brief Contains information about a module loaded into a process.
  struct Module {
    Module();
    ~Module();

    //! \brief The pathname used to load the module from disk.
    std::string name;
  };

  ProcessReader();
  ~ProcessReader();

  //! \return The modules loaded in the process. The first element (at index
  //!     `0`) corresponds to the main executable, and the final element
  //!     corresponds to the dynamic loader, dyld.
  const std::vector<Module>& Modules();

 private:
  void InitializeModules();

  std::vector<Module> modules_;
  bool initialized_modules_ = false;
  InitializationStateDcheck initialized_;

  DISALLOW_COPY_AND_ASSIGN(ProcessReader);
};

}  // namespace crashpad

#endif  // CRASHPAD_SNAPSHOT_FUCHSIA_PROCESS_READER_H_
