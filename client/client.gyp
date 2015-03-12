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
  'includes': [
    '../build/crashpad.gypi',
  ],
  'targets': [
    {
      'target_name': 'crashpad_client',
      'type': 'static_library',
      'dependencies': [
        '../compat/compat.gyp:crashpad_compat',
        '../third_party/mini_chromium/mini_chromium.gyp:base',
        '../util/util.gyp:crashpad_util',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'capture_context_mac.S',
        'capture_context_mac.h',
        'crash_report_database.cc',
        'crash_report_database.h',
        'crash_report_database_mac.mm',
        'crash_report_database_win.cc',
        'crashpad_client.h',
        'crashpad_client_mac.cc',
        'crashpad_info.cc',
        'crashpad_info.h',
        'settings.cc',
        'settings.h',
        'simple_string_dictionary.cc',
        'simple_string_dictionary.h',
        'simulate_crash.h',
        'simulate_crash_mac.cc',
        'simulate_crash_mac.h',
      ],
      'conditions': [
        ['OS=="win"', {
          'link_settings': {
            'libraries': [
              '-lrpcrt4.lib',
            ],
          },
          'sources!': [
            # Port to Win https://code.google.com/p/crashpad/issues/detail?id=13
            'settings.cc',
          ],
        }],
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '..',
        ],
      },
    },
    {
      'target_name': 'crashpad_client_test',
      'type': 'executable',
      'dependencies': [
        'crashpad_client',
        '../compat/compat.gyp:crashpad_compat',
        '../third_party/gtest/gtest.gyp:gtest',
        '../third_party/gtest/gtest.gyp:gtest_main',
        '../third_party/mini_chromium/mini_chromium.gyp:base',
        '../util/util.gyp:crashpad_util',
        '../util/util.gyp:crashpad_util_test_lib',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'capture_context_mac_test.cc',
        'crash_report_database_test.cc',
        'settings_test.cc',
        'simple_string_dictionary_test.cc',
        'simulate_crash_mac_test.cc',
      ],
      'conditions': [
        ['OS=="win"', {
          'sources!': [
            # Port to Win https://code.google.com/p/crashpad/issues/detail?id=13
            'settings_test.cc',
          ],
        }],
      ],
    },
  ],
}
