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

script_dir = os.path.dirname(__file__)
crashpad_dir = os.path.dirname(script_dir) if script_dir is not '' else '..'


# This script is primarily used from the waterfall so that the list of tests
# that are run is maintained in-tree, rather than in a separate infrastructure
# location in the recipe.
def main(args):
  if len(args) != 1:
    print >>sys.stderr, 'usage: run_tests.py {Debug|Release}'
    return 1;
  binary_dir = os.path.join(crashpad_dir, 'out', args[0])
  tests = [
      'client_test',
      'minidump_test',
      'snapshot_test',
      'util_test',
  ]
  for test in tests:
    print '-' * 80
    print test
    print '-' * 80
    subprocess.check_call(os.path.join(binary_dir, test))
  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
