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
  # Crashpad can build as a standalone project or as part of Chromium. When
  # building as a standalone project, it uses mini_chromium to provide the base
  # library, and uses its own copy of gtest in third_party. When building as
  # part of Chromium, it uses Chromium’s base library and copy of gtest. In
  # order for Crashpad’s .gyp files to reference the correct versions depending
  # on whether building standalone or as a part of Chromium, include this .gypi
  # file and reference the crashpad_in_chromium variable.

  'variables': {
    'variables': {
      # When building as a standalone project, build/gyp_crashpad.py sets
      # crashpad_standalone to 1, and this % assignment will not override it.
      # The variable will not be set when building as part of Chromium, so in
      # that case, this will define it with value 0.
      'crashpad_standalone%': 0,
    },

    'conditions': [
      ['crashpad_standalone!=0', {
        'crashpad_in_chromium': 0,
      }, {
        'crashpad_in_chromium': 1,
      }],
    ],

    'crashpad_in_chromium': '<(crashpad_in_chromium)',
  },
}
