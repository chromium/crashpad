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
        'file/fd_io.cc',
        'file/fd_io.h',
        'file/file_writer.cc',
        'file/file_writer.h',
        'file/string_file_writer.cc',
        'file/string_file_writer.h',
        'mac/launchd.h',
        'mac/launchd.mm',
        'mach/task_memory.cc',
        'mach/task_memory.h',
        'misc/initialization_state.h',
        'misc/initialization_state_dcheck.cc',
        'misc/initialization_state_dcheck.h',
        'misc/uuid.cc',
        'misc/uuid.h',
        'stdlib/cxx.h',
        'stdlib/objc.h',
        'stdlib/strlcpy.cc',
        'stdlib/strlcpy.h',
      ],
    },
    {
      'target_name': 'util_test_lib',
      'type': 'static_library',
      'dependencies': [
        '../compat/compat.gyp:compat',
        '../third_party/mini_chromium/mini_chromium/base/base.gyp:base',
        'util',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'test/errors.cc',
        'test/errors.h',
        'test/mac/mach_errors.cc',
        'test/mac/mach_errors.h',
      ],
    },
    {
      'target_name': 'util_test',
      'type': 'executable',
      'dependencies': [
        'util',
        'util_test_lib',
        '../compat/compat.gyp:compat',
        '../third_party/gtest/gtest.gyp:gtest',
        '../third_party/gtest/gtest.gyp:gtest_main',
        '../third_party/mini_chromium/mini_chromium/base/base.gyp:base',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'file/string_file_writer_test.cc',
        'mac/launchd_test.mm',
        'mach/task_memory_test.cc',
        'misc/initialization_state_dcheck_test.cc',
        'misc/initialization_state_test.cc',
        'misc/uuid_test.cc',
        'stdlib/strlcpy_test.cc',
      ],
    },
  ],
}
