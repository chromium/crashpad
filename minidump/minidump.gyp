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
      'target_name': 'minidump',
      'type': 'static_library',
      'dependencies': [
        '../compat/compat.gyp:compat',
        '../third_party/mini_chromium/mini_chromium/base/base.gyp:base',
        '../util/util.gyp:util',
      ],
      'export_dependent_settings': [
        '../compat/compat.gyp:compat',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'minidump_context.h',
        'minidump_context_writer.cc',
        'minidump_context_writer.h',
        'minidump_exception_writer.cc',
        'minidump_exception_writer.h',
        'minidump_extensions.cc',
        'minidump_extensions.h',
        'minidump_file_writer.cc',
        'minidump_file_writer.h',
        'minidump_memory_writer.cc',
        'minidump_memory_writer.h',
        'minidump_misc_info_writer.cc',
        'minidump_misc_info_writer.h',
        'minidump_module_writer.cc',
        'minidump_module_writer.h',
        'minidump_simple_string_dictionary_writer.cc',
        'minidump_simple_string_dictionary_writer.h',
        'minidump_stream_writer.cc',
        'minidump_stream_writer.h',
        'minidump_string_writer.cc',
        'minidump_string_writer.h',
        'minidump_system_info_writer.cc',
        'minidump_system_info_writer.h',
        'minidump_thread_writer.cc',
        'minidump_thread_writer.h',
        'minidump_writable.cc',
        'minidump_writable.h',
        'minidump_writer_util.cc',
        'minidump_writer_util.h',
      ],
    },
    {
      'target_name': 'minidump_test',
      'type': 'executable',
      'dependencies': [
        'minidump',
        '../third_party/gtest/gtest.gyp:gtest',
        '../third_party/gtest/gtest.gyp:gtest_main',
        '../third_party/mini_chromium/mini_chromium/base/base.gyp:base',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'minidump_context_test_util.cc',
        'minidump_context_test_util.h',
        'minidump_context_writer_test.cc',
        'minidump_exception_writer_test.cc',
        'minidump_file_writer_test.cc',
        'minidump_file_writer_test_util.cc',
        'minidump_file_writer_test_util.h',
        'minidump_memory_writer_test.cc',
        'minidump_memory_writer_test_util.cc',
        'minidump_memory_writer_test_util.h',
        'minidump_misc_info_writer_test.cc',
        'minidump_module_writer_test.cc',
        'minidump_simple_string_dictionary_writer_test.cc',
        'minidump_string_writer_test.cc',
        'minidump_system_info_writer_test.cc',
        'minidump_thread_writer_test.cc',
        'minidump_writable_test.cc',
      ],
    },
  ],
}
