# Copyright 2015 The Crashpad Authors. All rights reserved.
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
  'targets': [
    {
      # To support both Crashpad’s standalone build and its in-Chromium build,
      # Crashpad code depending on base should do so through this shim, which
      # will either get base from mini_chromium or Chromium depending on the
      # build type.
      'target_name': 'base',
      'type': 'none',
      'conditions': [
        ['crashpad_in_chromium==0', {
          'dependencies': [
            'mini_chromium/base/base.gyp:base',
          ],
          'export_dependent_settings': [
            'mini_chromium/base/base.gyp:base',
          ],
        }, {  # else: crashpad_in_chromium!=0
          'dependencies': [
            '<(DEPTH)/base/base.gyp:base',
          ],
          'export_dependent_settings': [
            '<(DEPTH)/base/base.gyp:base',
          ],
        }],
      ],
    },
  ],
}
