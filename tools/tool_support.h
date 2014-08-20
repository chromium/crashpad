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

#ifndef CRASHPAD_TOOLS_TOOL_SUPPORT_H_
#define CRASHPAD_TOOLS_TOOL_SUPPORT_H_

#include <string>

#include "base/basictypes.h"

namespace crashpad {

//! \brief Common functions used by command line tools.
class ToolSupport {
 public:
  //! \brief Handles `--version`.
  //!
  //! \param[in] me The tool’s name, the basename of `argv[0]`.
  static void Version(const std::string& me);

  //! \brief Prints the footer for `--help`.
  //!
  //! \param[in] me The tool’s name, the basename of `argv[0]`.
  static void UsageTail(const std::string& me);

  //! \brief Suggests using `--help` when a command line tool can’t make sense
  //!     of its arguments.
  //!
  //! \param[in] me The tool’s name, the basename of `argv[0]`.
  static void UsageHint(const std::string& me, const char* hint);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ToolSupport);
};

}  // namespace crashpad

#endif  // CRASHPAD_TOOLS_TOOL_SUPPORT_H_
