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
    '../../build/crashpad_in_chromium.gypi',
  ],
  'conditions': [
    ['crashpad_in_chromium==0', {
      'targets': [
        {
          'target_name': 'gmock',
          'type': 'static_library',
          'dependencies': [
            '../gtest/gtest.gyp:gtest',
          ],
          'include_dirs': [
            'gmock',
            'gmock/include',
          ],
          'sources': [
            'gmock/include/gmock/gmock-actions.h',
            'gmock/include/gmock/gmock-cardinalities.h',
            'gmock/include/gmock/gmock-generated-actions.h',
            'gmock/include/gmock/gmock-generated-function-mockers.h',
            'gmock/include/gmock/gmock-generated-matchers.h',
            'gmock/include/gmock/gmock-generated-nice-strict.h',
            'gmock/include/gmock/gmock-matchers.h',
            'gmock/include/gmock/gmock-more-actions.h',
            'gmock/include/gmock/gmock-more-matchers.h',
            'gmock/include/gmock/gmock-spec-builders.h',
            'gmock/include/gmock/gmock.h',
            'gmock/include/gmock/internal/gmock-generated-internal-utils.h',
            'gmock/include/gmock/internal/gmock-internal-utils.h',
            'gmock/include/gmock/internal/gmock-port.h',
            'gmock/src/gmock-all.cc',
            'gmock/src/gmock-cardinalities.cc',
            'gmock/src/gmock-internal-utils.cc',
            'gmock/src/gmock-matchers.cc',
            'gmock/src/gmock-spec-builders.cc',
            'gmock/src/gmock.cc',
          ],
          'sources!': [
            'gmock/src/gmock-all.cc',
          ],

          # gmock relies heavily on objects with static storage duration.
          'xcode_settings': {
            'WARNING_CFLAGS!': [
              '-Wexit-time-destructors',
            ],
          },
          'cflags!': [
            '-Wexit-time-destructors',
          ],

          'direct_dependent_settings': {
            'include_dirs': [
              'gmock/include',
            ],
            'conditions': [
              ['clang!=0', {
                # The MOCK_METHODn() macros do not specify “override”, which
                # triggers this warning in users: “error: 'Method' overrides a
                # member function but is not marked 'override'
                # [-Werror,-Winconsistent-missing-override]”. Suppress these
                # warnings, and add -Wno-unknown-warning-option because only
                # recent versions of clang (trunk r220703 and later, version
                # 3.6 and later) recognize it.
                'conditions': [
                  ['OS=="mac"', {
                    'xcode_settings': {
                      'WARNING_CFLAGS': [
                        '-Wno-inconsistent-missing-override',
                        '-Wno-unknown-warning-option',
                      ],
                    },
                  }],
                  ['OS=="linux"', {
                    'cflags': [
                      '-Wno-inconsistent-missing-override',
                      '-Wno-unknown-warning-option',
                    ],
                  }],
                ],
              }],
            ],
          },
          'export_dependent_settings': [
            '../gtest/gtest.gyp:gtest',
          ],
        },
        {
          'target_name': 'gmock_main',
          'type': 'static_library',
          'dependencies': [
            'gmock',
            '../gtest/gtest.gyp:gtest',
          ],
          'sources': [
            'gmock/src/gmock_main.cc',
          ],
        },
        {
          'target_name': 'gmock_test_executable',
          'type': 'none',
          'dependencies': [
            'gmock',
            '../gtest/gtest.gyp:gtest',
          ],
          'direct_dependent_settings': {
            'type': 'executable',
            'include_dirs': [
              'gmock',
            ],
          },
          'export_dependent_settings': [
            'gmock',
            '../gtest/gtest.gyp:gtest',
          ],
        },
        {
          'target_name': 'gmock_all_test',
          'dependencies': [
            'gmock_test_executable',
            'gmock_main',
          ],
          'sources': [
            'gmock/test/gmock-actions_test.cc',
            'gmock/test/gmock-cardinalities_test.cc',
            'gmock/test/gmock-generated-actions_test.cc',
            'gmock/test/gmock-generated-function-mockers_test.cc',
            'gmock/test/gmock-generated-internal-utils_test.cc',
            'gmock/test/gmock-generated-matchers_test.cc',
            'gmock/test/gmock-internal-utils_test.cc',
            'gmock/test/gmock-matchers_test.cc',
            'gmock/test/gmock-more-actions_test.cc',
            'gmock/test/gmock-nice-strict_test.cc',
            'gmock/test/gmock-port_test.cc',
            'gmock/test/gmock_test.cc',
          ],
        },
        {
          'target_name': 'gmock_link_test',
          'dependencies': [
            'gmock_test_executable',
            'gmock_main',
          ],
          'sources': [
            'gmock/test/gmock_link_test.cc',
            'gmock/test/gmock_link_test.h',
            'gmock/test/gmock_link2_test.cc',
          ],
        },
        {
          'target_name': 'gmock_spec_builders_test',
          'dependencies': [
            'gmock_test_executable',
          ],
          'sources': [
            'gmock/test/gmock-spec-builders_test.cc',
          ],
        },
        {
          'target_name': 'gmock_stress_test',
          'dependencies': [
            'gmock_test_executable',
          ],
          'sources': [
            'gmock/test/gmock_stress_test.cc',
          ],
        },
        {
          'target_name': 'gmock_all_tests',
          'type': 'none',
          'dependencies': [
            'gmock_all_test',
            'gmock_link_test',
            'gmock_spec_builders_test',
            'gmock_stress_test',
          ],
        },
      ],
    }, {  # else: crashpad_in_chromium!=0
      'targets': [
        {
          'target_name': 'gmock',
          'type': 'none',
          'dependencies': [
            '<(DEPTH)/testing/gmock.gyp:gmock',
          ],
          'export_dependent_settings': [
            '<(DEPTH)/testing/gmock.gyp:gmock',
          ],
        },
        {
          'target_name': 'gmock_main',
          'type': 'none',
          'dependencies': [
            '<(DEPTH)/testing/gmock.gyp:gmock_main',
          ],
          'export_dependent_settings': [
            '<(DEPTH)/testing/gmock.gyp:gmock_main',
          ],
        },
      ],
    }],
  ],
}
