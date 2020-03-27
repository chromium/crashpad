// Copyright 2020 The Crashpad Authors. All rights reserved.
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

#ifndef CRASHPAD_UTIL_IOS_EXCEPTION_PROCESSOR_H_
#define CRASHPAD_UTIL_IOS_EXCEPTION_PROCESSOR_H_

#include <stddef.h>

namespace crashpad {

// Installs the Objective-C exception preprocessor. This records UMA and crash
// keys for NSException objects. The preprocessor will also make fatal any
// exception that is not handled.
void InstallObjcExceptionPreprocessor();

}  // namespace crashpad

#endif  // CRASHPAD_UTIL_IOS_EXCEPTION_PROCESSOR_H_
