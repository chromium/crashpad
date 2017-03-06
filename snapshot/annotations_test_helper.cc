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

#include "snapshot/annotations_test_helper.h"

#include "base/strings/utf_string_conversions.h"
#include "gtest/gtest.h"
#include "snapshot/win/pe_image_annotations_reader.h"
#include "snapshot/win/pe_image_reader.h"
#include "snapshot/win/process_reader_win.h"

namespace crashpad {
namespace test {

// TODO(scottmg): Mac port.
std::map<std::string, std::string> GetAllAnnotations() {
  ProcessReaderWin process_reader;
  EXPECT_TRUE(process_reader.Initialize(GetCurrentProcess(),
                                        ProcessSuspensionState::kRunning));

  const std::vector<ProcessInfo::Module>& modules = process_reader.Modules();
  std::map<std::string, std::string> all_annotations_simple_map;
  for (const ProcessInfo::Module& module : modules) {
    PEImageReader pe_image_reader;
    pe_image_reader.Initialize(&process_reader,
                               module.dll_base,
                               module.size,
                               base::UTF16ToUTF8(module.name));
    PEImageAnnotationsReader module_annotations_reader(
        &process_reader, &pe_image_reader, module.name);
    std::map<std::string, std::string> module_annotations_simple_map =
        module_annotations_reader.SimpleMap();
    all_annotations_simple_map.insert(module_annotations_simple_map.begin(),
                                      module_annotations_simple_map.end());
  }

  return all_annotations_simple_map;
}

}  // namespace test
}  // namespace crashpad
