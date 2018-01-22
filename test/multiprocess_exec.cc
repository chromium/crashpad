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

namespace crashpad {
namespace test {

namespace internal {

namespace {

std::map<std::string, int(*)()>& GetMultiprocessFunctionMap() {
  static auto* map = new std::map<std::string, int(*)()>();
  return *map;
}

}  // namespace

AppendMultiprocessTest::AppendMultiprocessTest(const std::string& test_name,
                                               int (*main_function_pointer)()) {
  GetMultiprocessFunctionMap()[test_name] = main_function_pointer;
}

const char kChildTestProcess[] = "--child-test-process";

int InvokeMultiprocessChild(const std::string& test_name) {
  auto& functions = internal::GetMultiprocessFunctionMap();
  auto it = functions.find(test_name);
  if (it == functions.end()) {
    LOG(ERROR) << "child main " << test_name << " not registered";
    return -1;
  }
  return (*it->second)();
}

}  // namespace internal

void MultiprocessExec::SetChildTestMainProc(const std::string& procname) {
  std::string argv0 = GetMainArguments()[0];
  std::vector<std::string> rest(GetMainArguments().begin() + 1,
                                GetMainArguments().end());
  rest.push_back(internal::kChildTestProcess + std::string("=") + procname);
#if defined(OS_FUCHSIA)
  // Fuchsia's `namespace` command passes argv[0] as the non-namespaced path to
  // the binary, e.g. /tmp/<random_location>/pkg/bin/crashpad_test_test, whereas
  // to reinvoke, we need to use /pkg/bin/crashpad_test_test. Fix argv[0] here.
  const auto it = argv0.find("/pkg/");
  if (it != std::string::npos) {
    argv0 = argv0.substr(it);
  }
#endif  // OS_FUCHSIA

  SetChildCommand(base::FilePath(argv0), &rest);
}

}  // namespace test
}  // namespace crashpad
