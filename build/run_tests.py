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

import isolate_script

# This script is primarily used from the waterfall so that the list of tests
# that are run is maintained in-tree, rather than in a separate infrastructure
# location in the recipe.
def main(args):
  should_isolate = len(args) > 1 and args[0] == '--isolate'
  if should_isolate:
    args = args[1:]
  if len(args) != 1:
    print >> sys.stderr, 'usage: run_tests.py [--isolate] <binary_dir>'
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

  data = [
      '//build/run_tests.py',
      '//build/isolate_script.py',
      '//test/test_paths_test_data_root.txt',
      '//util/net/testdata/',
      '//util/net/http_transport_test_server.py',
  ]

  if sys.platform == 'win32':
    tests = sorted(tests + [
        'crashpad_handler_test',
    ])

    data += [
        'crash_other_program.exe',
        'crashpad_database_util.exe',
        'crashpad_handler.com',
        'crashpad_handler_test_extended_handler.exe',
        'crashpad_snapshot_test_crashing_child.exe',
        'crashpad_snapshot_test_dump_without_crashing.exe',
        'crashpad_snapshot_test.exe',
        'crashpad_snapshot_test_extra_memory_ranges.exe',
        'crashpad_snapshot_test_image_reader.exe',
        'crashpad_snapshot_test_image_reader_module.dll',
        'crashpad_snapshot_test_simple_annotations.exe',
        'crashpad_snapshot_test_module.dll',
        'crashpad_test_test_multiprocess_exec_test_child.exe',
        'crashpad_util_test_process_info_test_child.exe',
        'crashpad_util_test_safe_terminate_process_test_child.exe',
        'crashy_program.exe',
        'crashy_signal.exe',
        'crashy_z7_loader.exe',
        'fake_handler_that_crashes_at_startup.exe',
        'generate_dump.exe',
        'hanging_program.exe',
        'loader_lock_dll.dll',
        'self_destroying_program.exe',
        '//handler/win/z7_test.dll',
        '//snapshot/win/end_to_end_test.py',
    ]
    files = ['%s.exe' % test for test in tests] + data
  else:
    data += [
        'crashpad_snapshot_test_module.so',
        'crashpad_snapshot_test_module_crashy_initializer.so',
        'crashpad_snapshot_test_no_op',
        'crashpad_test_test_multiprocess_exec_test_child',
    ]
    files = tests + data

  if should_isolate:
    return isolate_script.write_files(
        crashpad_dir, binary_dir, 'run_tests',
        ['//build/run_tests.py', '.'], files)

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
