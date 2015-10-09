#!/usr/bin/env python

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
import platform
import subprocess
import sys


# This script is primarily used from the waterfall so that the list of tests
# that are run is maintained in-tree, rather than in a separate infrastructure
# location in the recipe.
def main(args):
  if len(args) != 1:
    print >> sys.stderr, \
        'usage: run_tests.py {Debug|Release|Debug_x64|Release_x64}'
    return 1

  crashpad_dir = \
      os.path.join(os.path.dirname(os.path.abspath(__file__)), os.pardir)

  # In a standalone Crashpad build, the out directory is in the Crashpad root.
  out_dir = os.path.join(crashpad_dir, 'out')
  if not os.path.exists(out_dir):
    # In an in-Chromium build, the out directory is in the Chromium root, and
    # the Crashpad root is in third_party/crashpad/crashpad relative to the
    # Chromium root.
    chromium_dir = os.path.join(crashpad_dir, os.pardir, os.pardir, os.pardir)
    out_dir = os.path.join(chromium_dir, 'out')
  if not os.path.exists(out_dir):
    raise Exception('could not determine out_dir', crashpad_dir)

  binary_dir = os.path.join(out_dir, args[0])

  tests = [
      'crashpad_client_test',
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
    name = 'snapshot/win/end_to_end_test.py'
    print '-' * 80
    print name
    print '-' * 80
    subprocess.check_call(
        [sys.executable, os.path.join(crashpad_dir, name), binary_dir])

  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
