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
      '505de88083136eefd056e5ee4ca0f01fe9b33de8',
  'crashpad/third_party/gtest/gtest':
      Var('chromium_git') + '/external/github.com/google/googletest@' +
      'd175c8bf823e709d570772b038757fadf63bc632',
  'crashpad/third_party/gyp/gyp':
      Var('chromium_git') + '/external/gyp@' +
      '5e2b3ddde7cda5eb6bc09a5546a76b00e49d888f',
  'crashpad/third_party/mini_chromium/mini_chromium':
      Var('chromium_git') + '/chromium/mini_chromium@' +
      'fa146c1264953b42baccfc261dc6aa59a1661d26',
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
