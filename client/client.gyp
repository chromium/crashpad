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

{
  'targets': [
    {
      'target_name': 'client',
      'type': 'static_library',
      'dependencies': [
        '../third_party/mini_chromium/mini_chromium/base/base.gyp:base',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'capture_context_mac.h',
        'capture_context_mac.S',
      ],
    },
    {
      'target_name': 'client_test',
      'type': 'executable',
      'dependencies': [
        'client',
        '../third_party/gtest/gtest.gyp:gtest',
        '../third_party/gtest/gtest.gyp:gtest_main',
        '../third_party/mini_chromium/mini_chromium/base/base.gyp:base',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'capture_context_mac_test.cc',
      ],
    },
  ],
}
