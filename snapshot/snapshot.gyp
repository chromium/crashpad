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
      'target_name': 'snapshot',
      'type': 'static_library',
      'dependencies': [
        '../compat/compat.gyp:compat',
        '../third_party/mini_chromium/mini_chromium/base/base.gyp:base',
        '../util/util.gyp:util',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'cpu_architecture.h',
        'cpu_context.cc',
        'cpu_context.h',
        'exception_snapshot.h',
        'mac/cpu_context_mac.cc',
        'mac/cpu_context_mac.h',
        'mac/exception_snapshot_mac.cc',
        'mac/exception_snapshot_mac.h',
        'mac/mach_o_image_reader.cc',
        'mac/mach_o_image_reader.h',
        'mac/mach_o_image_segment_reader.cc',
        'mac/mach_o_image_segment_reader.h',
        'mac/mach_o_image_symbol_table_reader.cc',
        'mac/mach_o_image_symbol_table_reader.h',
        'mac/memory_snapshot_mac.cc',
        'mac/memory_snapshot_mac.h',
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
        'mac/system_snapshot_mac.cc',
        'mac/system_snapshot_mac.h',
        'mac/thread_snapshot_mac.cc',
        'mac/thread_snapshot_mac.h',
        'memory_snapshot.h',
        'module_snapshot.h',
        'process_snapshot.h',
        'system_snapshot.h',
        'thread_snapshot.h',
      ],
    },
    {
      'target_name': 'snapshot_test',
      'type': 'executable',
      'dependencies': [
        'snapshot',
        '../compat/compat.gyp:compat',
        '../third_party/gtest/gtest.gyp:gtest',
        '../third_party/gtest/gtest.gyp:gtest_main',
        '../third_party/mini_chromium/mini_chromium/base/base.gyp:base',
        '../util/util.gyp:util',
        '../util/util.gyp:util_test_lib',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'mac/cpu_context_mac_test.cc',
        'mac/mach_o_image_reader_test.cc',
        'mac/mach_o_image_segment_reader_test.cc',
        'mac/process_reader_test.cc',
        'mac/process_types_test.cc',
        'mac/system_snapshot_mac_test.cc',
      ],
    },
  ],
}
