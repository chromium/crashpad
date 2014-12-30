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
  'conditions': [
    ['OS=="mac"', {
      'targets': [
        {
          'target_name': 'tool_support',
          'type': 'static_library',
          'dependencies': [
            '../third_party/mini_chromium/mini_chromium/base/base.gyp:base',
          ],
          'include_dirs': [
            '..',
          ],
          'sources': [
            'tool_support.cc',
            'tool_support.h',
          ],
        },
        {
          'target_name': 'catch_exception_tool',
          'type': 'executable',
          'dependencies': [
            'tool_support',
            '../compat/compat.gyp:compat',
            '../third_party/mini_chromium/mini_chromium/base/base.gyp:base',
            '../util/util.gyp:util',
          ],
          'include_dirs': [
            '..',
          ],
          'sources': [
            'mac/catch_exception_tool.cc',
          ],
        },
        {
          'target_name': 'exception_port_tool',
          'type': 'executable',
          'dependencies': [
            'tool_support',
            '../compat/compat.gyp:compat',
            '../third_party/mini_chromium/mini_chromium/base/base.gyp:base',
            '../util/util.gyp:util',
          ],
          'include_dirs': [
            '..',
          ],
          'sources': [
            'mac/exception_port_tool.cc',
          ],
        },
        {
          'target_name': 'generate_dump',
          'type': 'executable',
          'dependencies': [
            'tool_support',
            '../compat/compat.gyp:compat',
            '../minidump/minidump.gyp:minidump',
            '../snapshot/snapshot.gyp:snapshot',
            '../third_party/mini_chromium/mini_chromium/base/base.gyp:base',
            '../util/util.gyp:util',
          ],
          'include_dirs': [
            '..',
          ],
          'sources': [
            'generate_dump.cc',
          ],
        },
        {
          'target_name': 'on_demand_service_tool',
          'type': 'executable',
          'dependencies': [
            'tool_support',
            '../compat/compat.gyp:compat',
            '../third_party/mini_chromium/mini_chromium/base/base.gyp:base',
            '../util/util.gyp:util',
          ],
          'include_dirs': [
            '..',
          ],
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/CoreFoundation.framework',
              '$(SDKROOT)/System/Library/Frameworks/Foundation.framework',
            ],
          },
          'sources': [
            'mac/on_demand_service_tool.mm',
          ],
        },
        {
          'target_name': 'run_with_crashpad',
          'type': 'executable',
          'dependencies': [
            'tool_support',
            '../client/client.gyp:client',
            '../compat/compat.gyp:compat',
            '../third_party/mini_chromium/mini_chromium/base/base.gyp:base',
          ],
          'include_dirs': [
            '..',
          ],
          'sources': [
            'mac/run_with_crashpad.cc',
          ],
        },
      ],
    }, {
      'targets': [],
    }],
  ],
}
