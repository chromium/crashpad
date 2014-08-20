// Copyright 2014 The Crashpad Authors. All rights reserved.
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

#include "tools/tool_support.h"

#include <stdio.h>

#include "package.h"

namespace crashpad {

// static
void ToolSupport::Version(const std::string& me) {
  fprintf(stderr,
          "%s (%s) %s\n%s\n",
          me.c_str(),
          PACKAGE_NAME,
          PACKAGE_VERSION,
          PACKAGE_COPYRIGHT);
}

// static
void ToolSupport::UsageTail(const std::string& me) {
  fprintf(stderr,
          "\nReport %s bugs to\n%s\n%s home page: <%s>\n",
          me.c_str(),
          PACKAGE_BUGREPORT,
          PACKAGE_NAME,
          PACKAGE_URL);
}

// static
void ToolSupport::UsageHint(const std::string& me, const char* hint) {
  if (hint) {
    fprintf(stderr, "%s: %s\n", me.c_str(), hint);
  }
  fprintf(stderr, "Try '%s --help' for more information.\n", me.c_str());
}

}  // namespace crashpad
