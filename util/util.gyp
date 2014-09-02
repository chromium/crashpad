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
        'mac/checked_mach_address_range.cc',
        'mac/checked_mach_address_range.h',
        'mac/launchd.h',
        'mac/launchd.mm',
        'mac/mac_util.cc',
        'mac/mac_util.h',
        'mac/mach_o_image_segment_reader.cc',
        'mac/mach_o_image_segment_reader.h',
        'mac/service_management.cc',
        'mac/service_management.h',
        'mac/process_reader.cc',
        'mac/process_reader.h',
        'mac/process_types.cc',
        'mac/process_types.h',
        'mac/process_types/all.proctype',
        'mac/process_types/crashreporterclient.proctype',
        'mac/process_types/custom.cc',
        'mac/process_types/dyld_images.proctype',
        'mac/process_types/flavors.h',
        'mac/process_types/internal.h',
        'mac/process_types/loader.proctype',
        'mac/process_types/nlist.proctype',
        'mac/process_types/traits.h',
        'mach/bootstrap.cc',
        'mach/bootstrap.h',
        'mach/task_memory.cc',
        'mach/task_memory.h',
        'misc/initialization_state.h',
        'misc/initialization_state_dcheck.cc',
        'misc/initialization_state_dcheck.h',
        'misc/scoped_forbid_return.cc',
        'misc/scoped_forbid_return.h',
        'misc/uuid.cc',
        'misc/uuid.h',
        'numeric/checked_range.h',
        'numeric/in_range_cast.h',
        'numeric/safe_assignment.h',
        'posix/process_util.h',
        'posix/process_util_mac.cc',
        'stdlib/cxx.h',
        'stdlib/objc.h',
        'stdlib/pointer_container.h',
        'stdlib/strlcpy.cc',
        'stdlib/strlcpy.h',
        'stdlib/strnlen.cc',
        'stdlib/strnlen.h',
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
      'link_settings': {
        'libraries': [
          '$(SDKROOT)/usr/lib/libbsm.dylib',
        ],
      },
      'sources': [
        'test/errors.cc',
        'test/errors.h',
        'test/mac/mach_errors.cc',
        'test/mac/mach_errors.h',
        'test/mac/mach_multiprocess.cc',
        'test/mac/mach_multiprocess.h',
        'test/multiprocess.cc',
        'test/multiprocess.h',
        'test/posix/close_multiple.cc',
        'test/posix/close_multiple.h',
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
        'mac/checked_mach_address_range_test.cc',
        'mac/launchd_test.mm',
        'mac/mac_util_test.mm',
        'mac/mach_o_image_segment_reader_test.cc',
        'mac/process_reader_test.cc',
        'mac/process_types_test.cc',
        'mac/service_management_test.mm',
        'mach/bootstrap_test.cc',
        'mach/task_memory_test.cc',
        'misc/initialization_state_dcheck_test.cc',
        'misc/initialization_state_test.cc',
        'misc/scoped_forbid_return_test.cc',
        'misc/uuid_test.cc',
        'numeric/checked_range_test.cc',
        'numeric/in_range_cast_test.cc',
        'posix/process_util_test.cc',
        'stdlib/strlcpy_test.cc',
        'stdlib/strnlen_test.cc',
        'test/mac/mach_multiprocess_test.cc',
        'test/multiprocess_test.cc',
      ],
    },
  ],
}
