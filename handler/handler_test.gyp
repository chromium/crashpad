# Copyright 2017 The Crashpad Authors. All rights reserved.
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
  'includes': [
    '../build/crashpad.gypi',
  ],
  'targets': [
    {
      'target_name': 'crashpad_handler_test_extended_handler',
      'type': 'executable',
      'dependencies': [
        '../compat/compat.gyp:crashpad_compat',
        '../minidump/minidump_test.gyp:crashpad_minidump_test_lib',
        '../third_party/mini_chromium/mini_chromium.gyp:base',
        '../tools/tools.gyp:crashpad_tool_support',
        'handler.gyp:crashpad_handler_lib',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'crashpad_handler_test_extended_handler.cc',
      ],
    },
  ],
  'conditions': [
    ['OS=="win"', {
      'targets': [{
        # The handler is only tested on Windows for now.
        'target_name': 'crashpad_handler_test',
        'type': 'executable',
        'dependencies': [
          'crashpad_handler_test_extended_handler',
          'handler.gyp:crashpad_handler_lib',
          '../client/client.gyp:crashpad_client',
          '../compat/compat.gyp:crashpad_compat',
          '../test/test.gyp:crashpad_gtest_main',
          '../test/test.gyp:crashpad_test',
          '../third_party/gtest/gtest.gyp:gtest',
          '../third_party/mini_chromium/mini_chromium.gyp:base',
          '../util/util.gyp:crashpad_util',
        ],
        'include_dirs': [
          '..',
        ],
        'sources': [
          'crashpad_handler_test.cc',
        ],
      }],
    }],
  ],
}
