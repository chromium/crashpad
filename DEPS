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

vars = {
  'chromium_git': 'https://chromium.googlesource.com',
}

deps = {
  'crashpad/third_party/gmock/gmock':
      Var('chromium_git') + '/external/gmock@' +
      '29763965ab52f24565299976b936d1265cb6a271',  # svn r501
  'crashpad/third_party/gtest/gtest':
      Var('chromium_git') + '/external/gtest@' +
      '8245545b6dc9c4703e6496d1efd19e975ad2b038',  # svn r700
  'crashpad/third_party/gyp/gyp':
      Var('chromium_git') + '/external/gyp@' +
      '01528c7244837168a1c80f06ff60fa5a9793c824',
  'crashpad/third_party/mini_chromium/mini_chromium':
      Var('chromium_git') + '/chromium/mini_chromium@' +
      '8d42e2439aa0bd677dca64ba3070f3fa2353b7f2',
}

hooks = [
  {
    'name': 'gyp',
    'pattern': '\.gypi?$',
    'action': ['python', 'crashpad/build/gyp_crashpad.py'],
  },
]
