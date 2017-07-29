# Copyright 2014 The Crashpad Authors. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

vars = {
  'chromium_git': 'https://chromium.googlesource.com',
}

deps = {
  'buildtools':
      Var('chromium_git') + '/chromium/buildtools.git@' +
      'a7cc7a3e21a061975b33dcdcd81a9716ba614c3c',
  'crashpad/third_party/gtest/gtest':
      Var('chromium_git') + '/external/github.com/google/googletest@' +
      'd62d6c6556d96dda924382547c54a4b3afedb22c',
  'crashpad/third_party/gyp/gyp':
      Var('chromium_git') + '/external/gyp@' +
      'ffd524cefaad622e72995e852ffb0b18e83f8054',

  # TODO(scottmg): Consider pinning these. For now, we don't have any particular
  # reason to do so.
  'crashpad/third_party/llvm':
      Var('chromium_git') + '/external/llvm.org/llvm.git@HEAD',
  'crashpad/third_party/llvm/tools/clang':
      Var('chromium_git') + '/external/llvm.org/clang.git@HEAD',
  'crashpad/third_party/llvm/tools/lldb':
      Var('chromium_git') + '/external/llvm.org/lldb.git@HEAD',

  'crashpad/third_party/mini_chromium/mini_chromium':
      Var('chromium_git') + '/chromium/mini_chromium@' +
      'ee67585e3115982282b86e210939ead1791e696d',
  'crashpad/third_party/zlib/zlib':
      Var('chromium_git') + '/chromium/src/third_party/zlib@' +
      '13dc246a58e4b72104d35f9b1809af95221ebda7',
}

hooks = [
  {
    'name': 'clang_format_mac',
    'pattern': '.',
    'action': [
      'download_from_google_storage',
      '--platform=^darwin$',
      '--no_resume',
      '--no_auth',
      '--bucket=chromium-clang-format',
      '--sha1_file',
      'buildtools/mac/clang-format.sha1',
    ],
  },
  {
    'name': 'clang_format_win',
    'pattern': '.',
    'action': [
      'download_from_google_storage',
      '--platform=^win32$',
      '--no_resume',
      '--no_auth',
      '--bucket=chromium-clang-format',
      '--sha1_file',
      'buildtools/win/clang-format.exe.sha1',
    ],
  },
  {
    'name': 'clang_format_linux',
    'pattern': '.',
    'action': [
      'download_from_google_storage',
      '--platform=^linux2?$',
      '--no_resume',
      '--no_auth',
      '--bucket=chromium-clang-format',
      '--sha1_file',
      'buildtools/linux64/clang-format.sha1',
    ],
  },
  {
    'name': 'gn_mac',
    'pattern': '.',
    'action': [
      'download_from_google_storage',
      '--platform=^darwin$',
      '--no_resume',
      '--no_auth',
      '--bucket=chromium-gn',
      '--sha1_file',
      'buildtools/mac/gn.sha1',
    ],
  },
  {
    'name': 'gn_win',
    'pattern': '.',
    'action': [
      'download_from_google_storage',
      '--platform=^win32$',
      '--no_resume',
      '--no_auth',
      '--bucket=chromium-gn',
      '--sha1_file',
      'buildtools/win/gn.exe.sha1',
    ],
  },
  {
    'name': 'gn_linux',
    'pattern': '.',
    'action': [
      'download_from_google_storage',
      '--platform=^linux2?$',
      '--no_resume',
      '--no_auth',
      '--bucket=chromium-gn',
      '--sha1_file',
      'buildtools/linux64/gn.sha1',
    ],
  },
  {
    'name': 'gyp',
    'pattern': '\.gypi?$',
    'action': ['python', 'crashpad/build/gyp_crashpad.py'],
  },
]

recursedeps = [
  'buildtools',
]
