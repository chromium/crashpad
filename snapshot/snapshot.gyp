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
        'cpu_context_mac.cc',
        'cpu_context_mac.h',
        'exception_snapshot.h',
        'memory_snapshot.h',
        'memory_snapshot_mac.cc',
        'memory_snapshot_mac.h',
        'module_snapshot.h',
        'process_snapshot.h',
        'system_snapshot.h',
        'system_snapshot_mac.cc',
        'system_snapshot_mac.h',
        'thread_snapshot.h',
        'thread_snapshot_mac.cc',
        'thread_snapshot_mac.h',
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
        'cpu_context_mac_test.cc',
        'system_snapshot_mac_test.cc',
      ],
    },
  ],
}
