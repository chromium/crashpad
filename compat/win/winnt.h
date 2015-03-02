// Copyright 2015 The Crashpad Authors. All rights reserved.
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

#ifndef CRASHPAD_COMPAT_WIN_WINNT_H_
#define CRASHPAD_COMPAT_WIN_WINNT_H_

// https://msdn.microsoft.com/en-us/library/windows/desktop/aa373184.aspx:
// "Note that this structure definition was accidentally omitted from WinNT.h."
struct PROCESSOR_POWER_INFORMATION {
  ULONG Number;
  ULONG MaxMhz;
  ULONG CurrentMhz;
  ULONG MhzLimit;
  ULONG MaxIdleState;
  ULONG CurrentIdleState;
};

// include_next <winnt.h>
#include <../um/winnt.h>

#endif  // CRASHPAD_COMPAT_WIN_WINNT_H_
