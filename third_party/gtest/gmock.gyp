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
      'target_defaults': {
        # gmock relies heavily on objects with static storage duration.
        'xcode_settings': {
          'WARNING_CFLAGS!': [
            '-Wexit-time-destructors',
          ],
        },
        'cflags!': [
          '-Wexit-time-destructors',
        ],
      },

      'targets': [
        {
          'target_name': 'gmock',
          'type': 'static_library',
          'dependencies': [
            'gtest.gyp:gtest',
          ],
          'include_dirs': [
            'gtest/googlemock',
            'gtest/googlemock/include',
          ],
          'sources': [
            'gtest/googlemock/include/gmock/gmock-actions.h',
            'gtest/googlemock/include/gmock/gmock-cardinalities.h',
            'gtest/googlemock/include/gmock/gmock-generated-actions.h',
            'gtest/googlemock/include/gmock/gmock-generated-function-mockers.h',
            'gtest/googlemock/include/gmock/gmock-generated-matchers.h',
            'gtest/googlemock/include/gmock/gmock-generated-nice-strict.h',
            'gtest/googlemock/include/gmock/gmock-matchers.h',
            'gtest/googlemock/include/gmock/gmock-more-actions.h',
            'gtest/googlemock/include/gmock/gmock-more-matchers.h',
            'gtest/googlemock/include/gmock/gmock-spec-builders.h',
            'gtest/googlemock/include/gmock/gmock.h',
            'gtest/googlemock/include/gmock/internal/custom/gmock-generated-actions.h',
            'gtest/googlemock/include/gmock/internal/custom/gmock-matchers.h',
            'gtest/googlemock/include/gmock/internal/custom/gmock-port.h',
            'gtest/googlemock/include/gmock/internal/gmock-generated-internal-utils.h',
            'gtest/googlemock/include/gmock/internal/gmock-internal-utils.h',
            'gtest/googlemock/include/gmock/internal/gmock-port.h',
            'gtest/googlemock/src/gmock-all.cc',
            'gtest/googlemock/src/gmock-cardinalities.cc',
            'gtest/googlemock/src/gmock-internal-utils.cc',
            'gtest/googlemock/src/gmock-matchers.cc',
            'gtest/googlemock/src/gmock-spec-builders.cc',
            'gtest/googlemock/src/gmock.cc',
          ],
          'sources!': [
            'gtest/googlemock/src/gmock-all.cc',
          ],

          'direct_dependent_settings': {
            'include_dirs': [
              'gtest/googlemock/include',
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
            'gtest.gyp:gtest',
          ],
        },
        {
          'target_name': 'gmock_main',
          'type': 'static_library',
          'dependencies': [
            'gmock',
            'gtest.gyp:gtest',
          ],
          'sources': [
            'gtest/googlemock/src/gmock_main.cc',
          ],
        },
        {
          'target_name': 'gmock_test_executable',
          'type': 'none',
          'dependencies': [
            'gmock',
            'gtest.gyp:gtest',
          ],
          'direct_dependent_settings': {
            'type': 'executable',
            'include_dirs': [
              'gtest/googlemock',
            ],
          },
          'export_dependent_settings': [
            'gmock',
            'gtest.gyp:gtest',
          ],
        },
        {
          'target_name': 'gmock_all_test',
          'dependencies': [
            'gmock_test_executable',
            'gmock_main',
          ],
          'include_dirs': [
            'gtest/googletest',
          ],
          'sources': [
            'gtest/googlemock/test/gmock-actions_test.cc',
            'gtest/googlemock/test/gmock-cardinalities_test.cc',
            'gtest/googlemock/test/gmock-generated-actions_test.cc',
            'gtest/googlemock/test/gmock-generated-function-mockers_test.cc',
            'gtest/googlemock/test/gmock-generated-internal-utils_test.cc',
            'gtest/googlemock/test/gmock-generated-matchers_test.cc',
            'gtest/googlemock/test/gmock-internal-utils_test.cc',
            'gtest/googlemock/test/gmock-matchers_test.cc',
            'gtest/googlemock/test/gmock-more-actions_test.cc',
            'gtest/googlemock/test/gmock-nice-strict_test.cc',
            'gtest/googlemock/test/gmock-port_test.cc',
            'gtest/googlemock/test/gmock-spec-builders_test.cc',
            'gtest/googlemock/test/gmock_test.cc',
          ],
        },
        {
          'target_name': 'gmock_link_test',
          'dependencies': [
            'gmock_test_executable',
            'gmock_main',
          ],
          'sources': [
            'gtest/googlemock/test/gmock_link_test.cc',
            'gtest/googlemock/test/gmock_link_test.h',
            'gtest/googlemock/test/gmock_link2_test.cc',
          ],
        },
        {
          'target_name': 'gmock_stress_test',
          'dependencies': [
            'gmock_test_executable',
          ],
          'sources': [
            'gtest/googlemock/test/gmock_stress_test.cc',
          ],
        },
        {
          'target_name': 'gmock_all_tests',
          'type': 'none',
          'dependencies': [
            'gmock_all_test',
            'gmock_link_test',
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
