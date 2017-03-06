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

#include "client/annotations.h"

#include <map>
#include <string>

namespace crashpad {
namespace test {

//! \brief Gets all annotations from the current process.
//!
//! \return A key-value map of the annotations read from the current process.
std::map<std::string, std::string> GetAllAnnotations();

}  // namespace test
}  // namespace crashpad
