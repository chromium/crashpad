#!/usr/bin/env python
# coding: utf-8

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

import os
import subprocess
import sys


# This script is primarily used from the waterfall so that the list of tests
# that are run is maintained in-tree, rather than in a separate infrastructure
# location in the recipe.
def main(args):
  if len(args) != 1:
    print >> sys.stderr, 'usage: run_tests.py <binary_dir>'
    return 1

  crashpad_dir = \
      os.path.join(os.path.dirname(os.path.abspath(__file__)), os.pardir)
  binary_dir = args[0]

  # Tell 64-bit Windows tests where to find 32-bit test executables, for
  # cross-bitted testing. This relies on the fact that the GYP build by default
  # uses {Debug,Release} for the 32-bit build and {Debug,Release}_x64 for the
  # 64-bit build. This is not a universally valid assumption, and if it’s not
  # met, 64-bit tests that require 32-bit build output will disable themselves
  # dynamically.
  if (sys.platform == 'win32' and binary_dir.endswith('_x64') and
      'CRASHPAD_TEST_32_BIT_OUTPUT' not in os.environ):
    binary_dir_32 = binary_dir[:-4]
    if os.path.isdir(binary_dir_32):
      os.environ['CRASHPAD_TEST_32_BIT_OUTPUT'] = binary_dir_32

  tests = [
      'crashpad_client_test',
      'crashpad_handler_test',
      'crashpad_minidump_test',
      'crashpad_snapshot_test',
      'crashpad_test_test',
      'crashpad_util_test',
  ]

  for test in tests:
    print '-' * 80
    print test
    print '-' * 80
    subprocess.check_call(os.path.join(binary_dir, test))

  if sys.platform == 'win32':
    script = 'snapshot/win/end_to_end_test.py'
    print '-' * 80
    print script
    print '-' * 80
    subprocess.check_call(
        [sys.executable, os.path.join(crashpad_dir, script), binary_dir])

  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
