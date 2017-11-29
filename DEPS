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
      'f6d165d9d842ddd29056c127a5f3a3c5d8e0d2e3',
  'crashpad/third_party/gtest/gtest':
      Var('chromium_git') + '/external/github.com/google/googletest@' +
      '7b6561c56e353100aca8458d7bc49c4e0119bae8',
  'crashpad/third_party/gyp/gyp':
      Var('chromium_git') + '/external/gyp@' +
      'f72586209ecbf70b71ce690f2182ebe51669cbb3',
  'crashpad/third_party/mini_chromium/mini_chromium':
      Var('chromium_git') + '/chromium/mini_chromium@' +
      'dd0c3e9680ae3c4c22f2221a2a75e48dd4a562ec',
  'crashpad/third_party/zlib/zlib':
      Var('chromium_git') + '/chromium/src/third_party/zlib@' +
      '13dc246a58e4b72104d35f9b1809af95221ebda7',
}

hooks = [
  {
    'name': 'clang_format_mac',
    'pattern': '.',
    'condition': 'host_os == "mac"',
    'action': [
      'download_from_google_storage',
      '--no_resume',
      '--no_auth',
      '--bucket=chromium-clang-format',
      '--sha1_file',
      'buildtools/mac/clang-format.sha1',
    ],
  },
  {
    'name': 'clang_format_linux',
    'pattern': '.',
    'condition': 'host_os == "linux"',
    'action': [
      'download_from_google_storage',
      '--no_resume',
      '--no_auth',
      '--bucket=chromium-clang-format',
      '--sha1_file',
      'buildtools/linux64/clang-format.sha1',
    ],
  },
  {
    'name': 'clang_format_win',
    'pattern': '.',
    'condition': 'host_os == "win"',
    'action': [
      'download_from_google_storage',
      '--no_resume',
      '--no_auth',
      '--bucket=chromium-clang-format',
      '--sha1_file',
      'buildtools/win/clang-format.exe.sha1',
    ],
  },
  {
    'name': 'gn_mac',
    'pattern': '.',
    'condition': 'host_os == "mac"',
    'action': [
      'download_from_google_storage',
      '--no_resume',
      '--no_auth',
      '--bucket=chromium-gn',
      '--sha1_file',
      'buildtools/mac/gn.sha1',
    ],
  },
  {
    'name': 'gn_linux',
    'pattern': '.',
    'condition': 'host_os == "linux"',
    'action': [
      'download_from_google_storage',
      '--no_resume',
      '--no_auth',
      '--bucket=chromium-gn',
      '--sha1_file',
      'buildtools/linux64/gn.sha1',
    ],
  },
  {
    'name': 'gn_win',
    'pattern': '.',
    'condition': 'host_os == "win"',
    'action': [
      'download_from_google_storage',
      '--no_resume',
      '--no_auth',
      '--bucket=chromium-gn',
      '--sha1_file',
      'buildtools/win/gn.exe.sha1',
    ],
  },
  {
    # This uses “cipd install” so that mac-amd64 and linux-amd64 can coexist
    # peacefully. “cipd ensure” would remove the Linux package when running on a
    # macOS build host and vice-versa. https://crbug.com/789364.
    'name': 'fuchsia_clang_mac',
    'pattern': '.',
    'condition': 'checkout_fuchsia and host_os == "mac"',
    'action': [
      'cipd',
      'install',
      'fuchsia/clang/mac-amd64',
      'latest',
      '-root', 'crashpad/third_party/fuchsia/clang/mac-amd64',
      '-log-level', 'info',
    ],
  },
  {
    # This uses “cipd install” so that mac-amd64 and linux-amd64 can coexist
    # peacefully. “cipd ensure” would remove the macOS package when running on a
    # Linux build host and vice-versa. https://crbug.com/789364.
    'name': 'fuchsia_clang_linux',
    'pattern': '.',
    'condition': 'checkout_fuchsia and host_os == "linux"',
    'action': [
      'cipd',
      'install',
      'fuchsia/clang/linux-amd64',
      'latest',
      '-root', 'crashpad/third_party/fuchsia/clang/linux-amd64',
      '-log-level', 'info',
    ],
  },
  {
    # The SDK is keyed to the host system because it contains build tools.
    # Currently, linux-amd64 is the only SDK published (see
    # https://chrome-infra-packages.appspot.com/#/?path=fuchsia/sdk). As long as
    # this is the case, use that SDK package even on other build hosts. The
    # sysroot (containing headers and libraries) and other components are
    # related to the target and should be functional with an appropriate
    # toolchain that runs on the build host (fuchsia_clang, above).
    'name': 'fuchsia_sdk',
    'pattern': '.',
    'condition': 'checkout_fuchsia',
    'action': [
      'cipd',
      'install',
      'fuchsia/sdk/linux-amd64',
      'latest',
      '-root', 'crashpad/third_party/fuchsia/sdk/linux-amd64',
      '-log-level', 'info',
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
