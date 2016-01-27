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
  # Crashpad can obtain dependencies in three different ways, directed by the
  # crashpad_standalone GYP variable. It may have these values:
  #   standalone
  #     A “standalone” Crashpad build, where the dependencies are in the
  #     Crashpad tree. third_party/mini_chromium and third_party/gtest provide
  #     the base and gtest libraries.
  #   chromium
  #     An in-Chromium build, where Crashpad is within the Chromium tree.
  #     Chromium provides its own base library and its copy of the gtest
  #     library.
  #   external
  #     A build with external dependencies. mini_chromium provides the base
  #     library, but it’s located outside of the Crashpad tree, as is gtest.
  #
  # In order for Crashpad’s .gyp files to reference the correct versions
  # depending on how dependencies are being provided, include this .gypi file
  # and reference the crashpad_dependencies variable.

  'variables': {
    # When building as a standalone project or with external dependencies,
    # build/gyp_crashpad.py sets crashpad_dependencies to "standalone" or
    # "external", and this % assignment will not override it. The variable will
    # not be set by anything else when building as part of Chromium, so in that
    # case, this will define it with value "chromium".
    'crashpad_dependencies%': 'chromium',
  },
}
