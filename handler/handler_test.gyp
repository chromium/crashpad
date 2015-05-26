# Copyright 2015 The Crashpad Authors. All rights reserved.
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
  'targets': [],
  'conditions': [
    ['OS=="win"', {
      'targets': [
        {
          'target_name': 'crashpad_handler_test',
          'type': 'executable',
          'dependencies': [
            'handler.gyp:crashpad_handler',
            '../compat/compat.gyp:crashpad_compat',
            '../test/test.gyp:crashpad_test',
            '../third_party/gtest/gtest.gyp:gtest',
            '../third_party/gtest/gtest.gyp:gtest_main',
            '../third_party/mini_chromium/mini_chromium.gyp:base',
            '../util/util.gyp:crashpad_util',
          ],
          'include_dirs': [
            '..',
          ],
          'sources': [
            'win/registration_pipe_state_test.cc',
            'win/registration_server_test.cc',
            'win/registration_test_base.cc',
            'win/registration_test_base.h',
          ],
        },
      ],
    }],
  ],
}
