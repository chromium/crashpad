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

#include "handler/handler_main.h"

#include "build/build_config.h"
#include "tools/tool_support.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

#if defined(OS_MACOSX)
int main(int argc, char* argv[]) {
  return crashpad::HandlerMain(argc, argv);
}
#elif defined(OS_WIN)
#if defined(WIN_CONSOLE_APP)
int wmain(int argc, wchar_t* argv[]) {
  return crashpad::ToolSupport::Wmain(argc, argv, crashpad::HandlerMain);
}
#else
int WINAPI WinMain(HINSTANCE instance,
                   HINSTANCE prev_instance,
                   LPSTR cmd_line,
                   int cmd_show) {
  return crashpad::ToolSupport::Wmain(__argc, __wargv, crashpad::HandlerMain);
}
#endif  // WIN_CONSOLE_APP
#endif  // OS_MACOSX
