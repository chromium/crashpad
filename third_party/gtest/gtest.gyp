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
        # gtest relies heavily on objects with static storage duration.
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
          'target_name': 'gtest',
          'type': 'static_library',
          'include_dirs': [
            'gtest/googletest',
            'gtest/googletest/include',
          ],
          'sources': [
            'gtest/googletest/include/gtest/gtest-death-test.h',
            'gtest/googletest/include/gtest/gtest-message.h',
            'gtest/googletest/include/gtest/gtest-param-test.h',
            'gtest/googletest/include/gtest/gtest-printers.h',
            'gtest/googletest/include/gtest/gtest-spi.h',
            'gtest/googletest/include/gtest/gtest-test-part.h',
            'gtest/googletest/include/gtest/gtest-typed-test.h',
            'gtest/googletest/include/gtest/gtest.h',
            'gtest/googletest/include/gtest/gtest_pred_impl.h',
            'gtest/googletest/include/gtest/gtest_prod.h',
            'gtest/googletest/include/gtest/internal/custom/gtest-port.h',
            'gtest/googletest/include/gtest/internal/custom/gtest-printers.h',
            'gtest/googletest/include/gtest/internal/custom/gtest.h',
            'gtest/googletest/include/gtest/internal/gtest-death-test-internal.h',
            'gtest/googletest/include/gtest/internal/gtest-filepath.h',
            'gtest/googletest/include/gtest/internal/gtest-internal.h',
            'gtest/googletest/include/gtest/internal/gtest-linked_ptr.h',
            'gtest/googletest/include/gtest/internal/gtest-param-util-generated.h',
            'gtest/googletest/include/gtest/internal/gtest-param-util.h',
            'gtest/googletest/include/gtest/internal/gtest-port-arch.h',
            'gtest/googletest/include/gtest/internal/gtest-port.h',
            'gtest/googletest/include/gtest/internal/gtest-string.h',
            'gtest/googletest/include/gtest/internal/gtest-tuple.h',
            'gtest/googletest/include/gtest/internal/gtest-type-util.h',
            'gtest/googletest/src/gtest-all.cc',
            'gtest/googletest/src/gtest-death-test.cc',
            'gtest/googletest/src/gtest-filepath.cc',
            'gtest/googletest/src/gtest-internal-inl.h',
            'gtest/googletest/src/gtest-port.cc',
            'gtest/googletest/src/gtest-printers.cc',
            'gtest/googletest/src/gtest-test-part.cc',
            'gtest/googletest/src/gtest-typed-test.cc',
            'gtest/googletest/src/gtest.cc',
          ],
          'sources!': [
            'gtest/googletest/src/gtest-all.cc',
          ],

          'direct_dependent_settings': {
            'include_dirs': [
              'gtest/googletest/include',
            ],
          },
        },
        {
          'target_name': 'gtest_main',
          'type': 'static_library',
          'dependencies': [
            'gtest',
          ],
          'sources': [
            'gtest/googletest/src/gtest_main.cc',
          ],
        },
        {
          'target_name': 'gtest_test_executable',
          'type': 'none',
          'dependencies': [
            'gtest',
          ],
          'direct_dependent_settings': {
            'type': 'executable',
            'include_dirs': [
              'gtest/googletest',
            ],
          },
          'export_dependent_settings': [
            'gtest',
          ],
        },
        {
          'target_name': 'gtest_all_test',
          'dependencies': [
            'gtest_test_executable',
            'gtest_main',
          ],
          'sources': [
            'gtest/googletest/test/gtest-death-test_test.cc',
            'gtest/googletest/test/gtest-filepath_test.cc',
            'gtest/googletest/test/gtest-linked_ptr_test.cc',
            'gtest/googletest/test/gtest-message_test.cc',
            'gtest/googletest/test/gtest-options_test.cc',
            'gtest/googletest/test/gtest-port_test.cc',
            'gtest/googletest/test/gtest-printers_test.cc',
            'gtest/googletest/test/gtest-test-part_test.cc',
            'gtest/googletest/test/gtest-typed-test2_test.cc',
            'gtest/googletest/test/gtest-typed-test_test.cc',
            'gtest/googletest/test/gtest-typed-test_test.h',
            'gtest/googletest/test/gtest_main_unittest.cc',
            'gtest/googletest/test/gtest_pred_impl_unittest.cc',
            'gtest/googletest/test/gtest_prod_test.cc',
            'gtest/googletest/test/gtest_unittest.cc',
            'gtest/googletest/test/production.cc',
            'gtest/googletest/test/production.h',
          ],
        },
        {
          'target_name': 'gtest_environment_test',
          'dependencies': [
            'gtest_test_executable',
          ],
          'sources': [
            'gtest/googletest/test/gtest_environment_test.cc',
          ],
        },
        {
          'target_name': 'gtest_listener_test',
          'dependencies': [
            'gtest_test_executable',
          ],
          'sources': [
            'gtest/googletest/test/gtest-listener_test.cc',
          ],
        },
        {
          'target_name': 'gtest_no_test',
          'dependencies': [
            'gtest_test_executable',
          ],
          'sources': [
            'gtest/googletest/test/gtest_no_test_unittest.cc',
          ],
        },
        {
          'target_name': 'gtest_param_test',
          'dependencies': [
            'gtest_test_executable',
          ],
          'sources': [
            'gtest/googletest/test/gtest-param-test2_test.cc',
            'gtest/googletest/test/gtest-param-test_test.cc',
            'gtest/googletest/test/gtest-param-test_test.h',
          ],
        },
        {
          'target_name': 'gtest_premature_exit_test',
          'dependencies': [
            'gtest_test_executable',
          ],
          'sources': [
            'gtest/googletest/test/gtest_premature_exit_test.cc',
          ],
        },
        {
          'target_name': 'gtest_repeat_test',
          'dependencies': [
            'gtest_test_executable',
          ],
          'sources': [
            'gtest/googletest/test/gtest_repeat_test.cc',
          ],
        },
        {
          'target_name': 'gtest_sole_header_test',
          'dependencies': [
            'gtest_test_executable',
            'gtest_main',
          ],
          'sources': [
            'gtest/googletest/test/gtest_sole_header_test.cc',
          ],
        },
        {
          'target_name': 'gtest_stress_test',
          'dependencies': [
            'gtest_test_executable',
          ],
          'sources': [
            'gtest/googletest/test/gtest_stress_test.cc',
          ],
        },
        {
          'target_name': 'gtest_unittest_api_test',
          'dependencies': [
            'gtest_test_executable',
          ],
          'sources': [
            'gtest/googletest/test/gtest-unittest-api_test.cc',
          ],
        },
        {
          'target_name': 'gtest_all_tests',
          'type': 'none',
          'dependencies': [
            'gtest_all_test',
            'gtest_environment_test',
            'gtest_listener_test',
            'gtest_no_test',
            'gtest_param_test',
            'gtest_premature_exit_test',
            'gtest_repeat_test',
            'gtest_sole_header_test',
            'gtest_stress_test',
            'gtest_unittest_api_test',
          ],
        },
      ],
    }, {  # else: crashpad_in_chromium!=0
      'targets': [
        {
          'target_name': 'gtest',
          'type': 'none',
          'dependencies': [
            '<(DEPTH)/testing/gtest.gyp:gtest',
          ],
          'export_dependent_settings': [
            '<(DEPTH)/testing/gtest.gyp:gtest',
          ],
        },
        {
          'target_name': 'gtest_main',
          'type': 'none',
          'dependencies': [
            '<(DEPTH)/testing/gtest.gyp:gtest_main',
          ],
          'export_dependent_settings': [
            '<(DEPTH)/testing/gtest.gyp:gtest_main',
          ],
        },
      ],
    }],
  ],
}
