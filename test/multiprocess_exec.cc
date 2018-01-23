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

#include "test/multiprocess_exec.h"

#include <map>

#include "base/logging.h"
#include "test/main_arguments.h"
#include "util/stdlib/map_insert.h"

namespace crashpad {
namespace test {

namespace internal {

namespace {

std::map<std::string, int(*)()>* GetMultiprocessFunctionMap() {
  static auto* map = new std::map<std::string, int(*)()>();
  return map;
}

}  // namespace

AppendMultiprocessTest::AppendMultiprocessTest(const std::string& test_name,
                                               int (*main_function_pointer)()) {
  CHECK(MapInsertOrReplace(
      GetMultiprocessFunctionMap(), test_name, main_function_pointer, nullptr))
      << test_name << " already registered";
}

int CheckedInvokeMultiprocessChild(const std::string& test_name) {
  auto* functions = internal::GetMultiprocessFunctionMap();
  auto it = functions->find(test_name);
  CHECK(it != functions->end())
      << "child main " << test_name << " not registered";
  return (*it->second)();
}

}  // namespace internal

void MultiprocessExec::SetChildTestMainFunction(
    const std::string& function_name) {
  std::string argv0 = GetMainArguments()[0];
  std::vector<std::string> rest(GetMainArguments().begin() + 1,
                                GetMainArguments().end());
  rest.push_back(internal::kChildTestFunction + function_name);
  SetChildCommand(base::FilePath(argv0), &rest);
}

}  // namespace test
}  // namespace crashpad
