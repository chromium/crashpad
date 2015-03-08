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
      'target_name': 'util',
      'type': 'static_library',
      'dependencies': [
        '../compat/compat.gyp:compat',
        '../third_party/mini_chromium/mini_chromium/base/base.gyp:base',
      ],
      'include_dirs': [
        '..',
        '<(INTERMEDIATE_DIR)',
      ],
      'sources': [
        'file/file_io.cc',
        'file/file_io.h',
        'file/file_io_posix.cc',
        'file/file_io_win.cc',
        'file/file_reader.cc',
        'file/file_reader.h',
        'file/file_seeker.cc',
        'file/file_seeker.h',
        'file/file_writer.cc',
        'file/file_writer.h',
        'file/string_file.cc',
        'file/string_file.h',
        'mac/checked_mach_address_range.cc',
        'mac/checked_mach_address_range.h',
        'mac/launchd.h',
        'mac/launchd.mm',
        'mac/mac_util.cc',
        'mac/mac_util.h',
        'mac/service_management.cc',
        'mac/service_management.h',
        'mac/xattr.cc',
        'mac/xattr.h',
        'mach/child_port.defs',
        'mach/child_port_handshake.cc',
        'mach/child_port_handshake.h',
        'mach/child_port_server.cc',
        'mach/child_port_server.h',
        'mach/child_port_types.h',
        'mach/composite_mach_message_server.cc',
        'mach/composite_mach_message_server.h',
        'mach/exc_client_variants.cc',
        'mach/exc_client_variants.h',
        'mach/exc_server_variants.cc',
        'mach/exc_server_variants.h',
        'mach/exception_behaviors.cc',
        'mach/exception_behaviors.h',
        'mach/exception_ports.cc',
        'mach/exception_ports.h',
        'mach/mach_extensions.cc',
        'mach/mach_extensions.h',
        'mach/mach_message.cc',
        'mach/mach_message.h',
        'mach/mach_message_server.cc',
        'mach/mach_message_server.h',
        'mach/notify_server.cc',
        'mach/notify_server.h',
        'mach/scoped_task_suspend.cc',
        'mach/scoped_task_suspend.h',
        'mach/symbolic_constants_mach.cc',
        'mach/symbolic_constants_mach.h',
        'mach/task_for_pid.cc',
        'mach/task_for_pid.h',
        'mach/task_memory.cc',
        'mach/task_memory.h',
        'misc/clock.h',
        'misc/clock_mac.cc',
        'misc/clock_posix.cc',
        'misc/clock_win.cc',
        'misc/initialization_state.h',
        'misc/initialization_state_dcheck.cc',
        'misc/initialization_state_dcheck.h',
        'misc/scoped_forbid_return.cc',
        'misc/scoped_forbid_return.h',
        'misc/symbolic_constants_common.h',
        'misc/uuid.cc',
        'misc/uuid.h',
        'net/http_body.cc',
        'net/http_body.h',
        'net/http_headers.cc',
        'net/http_headers.h',
        'net/http_multipart_builder.cc',
        'net/http_multipart_builder.h',
        'net/http_transport.cc',
        'net/http_transport.h',
        'net/http_transport_mac.mm',
        'net/http_transport_win.cc',
        'numeric/checked_range.h',
        'numeric/in_range_cast.h',
        'numeric/int128.h',
        'numeric/safe_assignment.h',
        'posix/close_multiple.cc',
        'posix/close_multiple.h',
        'posix/close_stdio.cc',
        'posix/close_stdio.h',
        'posix/drop_privileges.cc',
        'posix/drop_privileges.h',
        'posix/process_info.h',
        'posix/process_info_mac.cc',
        'posix/symbolic_constants_posix.cc',
        'posix/symbolic_constants_posix.h',
        'stdlib/cxx.h',
        'stdlib/objc.h',
        'stdlib/pointer_container.h',
        'stdlib/string_number_conversion.cc',
        'stdlib/string_number_conversion.h',
        'stdlib/strlcpy.cc',
        'stdlib/strlcpy.h',
        'stdlib/strnlen.cc',
        'stdlib/strnlen.h',
        'synchronization/semaphore_mac.cc',
        'synchronization/semaphore_posix.cc',
        'synchronization/semaphore_win.cc',
        'synchronization/semaphore.h',
        'win/process_info.cc',
        'win/process_info.h',
        'win/scoped_handle.cc',
        'win/scoped_handle.h',
        'win/time.cc',
        'win/time.h',
      ],
      'conditions': [
        ['OS=="mac"', {
          'conditions': [
            ['GENERATOR=="ninja"', {
              # ninja’s rules can’t deal with sources that have paths relative
              # to environment variables like SDKROOT. Copy the .defs files out
              # of SDKROOT and into a place they can be referenced without any
              # environment variables.
              'copies': [
                {
                  'destination': '<(INTERMEDIATE_DIR)/util/mach',
                  'files': [
                    '$(SDKROOT)/usr/include/mach/exc.defs',
                    '$(SDKROOT)/usr/include/mach/mach_exc.defs',
                    '$(SDKROOT)/usr/include/mach/notify.defs',
                  ],
                },
              ],
              'sources': [
                '<(INTERMEDIATE_DIR)/util/mach/exc.defs',
                '<(INTERMEDIATE_DIR)/util/mach/mach_exc.defs',
                '<(INTERMEDIATE_DIR)/util/mach/notify.defs',
              ],
            }, {  # else: GENERATOR!="ninja"
              # The Xcode generator does copies after rules, so the above trick
              # won’t work, but its rules tolerate sources with SDKROOT-relative
              # paths.
              'sources': [
                '$(SDKROOT)/usr/include/mach/exc.defs',
                '$(SDKROOT)/usr/include/mach/mach_exc.defs',
                '$(SDKROOT)/usr/include/mach/notify.defs',
              ],
            }],
          ],
          'rules': [
            {
              'rule_name': 'mig',
              'extension': 'defs',
              'inputs': [
                'mach/mig.py',
              ],
              'outputs': [
                '<(INTERMEDIATE_DIR)/util/mach/<(RULE_INPUT_ROOT)User.c',
                '<(INTERMEDIATE_DIR)/util/mach/<(RULE_INPUT_ROOT)Server.c',
                '<(INTERMEDIATE_DIR)/util/mach/<(RULE_INPUT_ROOT).h',
                '<(INTERMEDIATE_DIR)/util/mach/<(RULE_INPUT_ROOT)Server.h',
              ],
              'action': [
                'python', '<@(_inputs)', '<(RULE_INPUT_PATH)', '<@(_outputs)'
              ],
              'process_outputs_as_sources': 1,
            },
          ],
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/CoreFoundation.framework',
              '$(SDKROOT)/System/Library/Frameworks/Foundation.framework',
              '$(SDKROOT)/System/Library/Frameworks/IOKit.framework',
            ],
          },
        }],
        ['OS=="win"', {
          'link_settings': {
            'libraries': [
              '-lwinhttp.lib',
            ],
          },
        }],
      ],
    },
    {
      'target_name': 'util_test_lib',
      'type': 'static_library',
      'dependencies': [
        '../compat/compat.gyp:compat',
        '../third_party/gtest/gtest.gyp:gtest',
        '../third_party/mini_chromium/mini_chromium/base/base.gyp:base',
        'util',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'test/errors.cc',
        'test/errors.h',
        'test/executable_path.h',
        'test/executable_path_mac.cc',
        'test/executable_path_win.cc',
        'test/mac/dyld.h',
        'test/mac/mach_errors.cc',
        'test/mac/mach_errors.h',
        'test/mac/mach_multiprocess.cc',
        'test/mac/mach_multiprocess.h',
        'test/multiprocess.h',
        'test/multiprocess_exec.h',
        'test/multiprocess_exec_posix.cc',
        'test/multiprocess_exec_win.cc',
        'test/multiprocess_posix.cc',
        'test/scoped_temp_dir.cc',
        'test/scoped_temp_dir.h',
        'test/scoped_temp_dir_posix.cc',
        'test/scoped_temp_dir_win.cc',
      ],
      'conditions': [
        ['OS=="mac"', {
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/usr/lib/libbsm.dylib',
            ],
          },
        }],
      ],
    },
    {
      'target_name': 'util_test',
      'type': 'executable',
      'dependencies': [
        'util',
        'util_test_lib',
        'util_test_multiprocess_exec_test_child',
        '../compat/compat.gyp:compat',
        '../third_party/gmock/gmock.gyp:gmock',
        '../third_party/gtest/gtest.gyp:gtest',
        '../third_party/gtest/gtest.gyp:gtest_main',
        '../third_party/mini_chromium/mini_chromium/base/base.gyp:base',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'file/string_file_test.cc',
        'mac/checked_mach_address_range_test.cc',
        'mac/launchd_test.mm',
        'mac/mac_util_test.mm',
        'mac/service_management_test.mm',
        'mac/xattr_test.cc',
        'mach/child_port_handshake_test.cc',
        'mach/child_port_server_test.cc',
        'mach/composite_mach_message_server_test.cc',
        'mach/exc_client_variants_test.cc',
        'mach/exc_server_variants_test.cc',
        'mach/exception_behaviors_test.cc',
        'mach/exception_ports_test.cc',
        'mach/mach_extensions_test.cc',
        'mach/mach_message_server_test.cc',
        'mach/mach_message_test.cc',
        'mach/notify_server_test.cc',
        'mach/scoped_task_suspend_test.cc',
        'mach/symbolic_constants_mach_test.cc',
        'mach/task_memory_test.cc',
        'misc/clock_test.cc',
        'misc/initialization_state_dcheck_test.cc',
        'misc/initialization_state_test.cc',
        'misc/scoped_forbid_return_test.cc',
        'misc/uuid_test.cc',
        'net/http_body_test.cc',
        'net/http_body_test_util.cc',
        'net/http_body_test_util.h',
        'net/http_multipart_builder_test.cc',
        'net/http_transport_test.cc',
        'numeric/checked_range_test.cc',
        'numeric/in_range_cast_test.cc',
        'numeric/int128_test.cc',
        'posix/process_info_test.cc',
        'posix/symbolic_constants_posix_test.cc',
        'stdlib/string_number_conversion_test.cc',
        'stdlib/strlcpy_test.cc',
        'stdlib/strnlen_test.cc',
        'synchronization/semaphore_test.cc',
        'test/executable_path_test.cc',
        'test/mac/mach_multiprocess_test.cc',
        'test/multiprocess_exec_test.cc',
        'test/multiprocess_posix_test.cc',
        'test/scoped_temp_dir_test.cc',
        'win/process_info_test.cc',
        'win/time_test.cc',
      ],
      'conditions': [
        ['OS=="mac"', {
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/Foundation.framework',
            ],
          },
        }],
        ['OS=="win"', {
          'link_settings': {
            'libraries': [
              '-lrpcrt4.lib',
            ],
          },
        }],
      ],
    },
    {
      'target_name': 'util_test_multiprocess_exec_test_child',
      'type': 'executable',
      'sources': [
        'test/multiprocess_exec_test_child.cc',
      ],
    },
  ],
  'conditions': [
    ['OS=="win"', {
      'targets': [
        {
          'target_name': 'util_test_process_info_test_child',
          'type': 'executable',
          'sources': [
            'win/process_info_test_child.cc',
          ],
          # Set an unusually high load address to make sure that the main
          # executable still appears as the first element in
          # ProcessInfo::Modules().
          'msvs_settings': {
            'VCLinkerTool': {
              'AdditionalOptions': [
                '/BASE:0x78000000',
                '/FIXED',
              ],
            },
          },
        },
      ]
    }],
  ],
}
