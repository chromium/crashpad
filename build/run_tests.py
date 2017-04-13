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

  tests = [
      'crashpad_client_test',
      'crashpad_minidump_test',
      'crashpad_snapshot_test',
      'crashpad_test_test',
      'crashpad_util_test',
  ]

  if sys.platform == 'win32':
    tests.append('crashpad_handler_test')
    tests = sorted(tests)

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
