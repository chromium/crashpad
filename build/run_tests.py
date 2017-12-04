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

from __future__ import print_function

import os
import pipes
import subprocess
import sys
import uuid

CRASHPAD_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                            os.pardir)


def _BinaryDirLooksLikeFuchsiaBuild(binary_dir):
  """Checks whether the provided output directory targets Fuchsia."""
  value = subprocess.check_output(
      ['gn', 'args', binary_dir, '--list=target_os', '--short'])
  return 'target_os = "fuchsia"' in value


def _GenerateFuchsiaRuntimeDepsFiles(binary_dir, tests):
  """Ensures a <binary_dir>/<test>.runtime_deps file exists for each test."""
  targets_file = os.path.abspath(os.path.join(binary_dir, 'targets.txt'))
  with open(targets_file, 'wb') as f:
    f.write('//:' + '\n//:'.join(tests) + '\n')
  subprocess.check_call(
      ['gn', 'gen', binary_dir, '--runtime-deps-list-file=' + targets_file])


def _HandleOutputFromFuchsiaLogListener(process, done_message):
  """Pass through the output from |process| (which should be an instance of
  Fuchsia's loglistener) until a special termination |done_message| is
  encountered.

  Also attempts to determine if any tests failed by inspecting the log output,
  and returns False if there were failures.
  """
  success = True
  while True:
    line = process.stdout.readline().rstrip()
    if 'FAILED TEST' in line:
      success = False
    elif done_message in line and 'echo ' not in line:
      break
    print(line)
  return success


def _RuntimeDepsPathToFuchsiaTargetPath(runtime_dep):
  """Determines the target location for a given Fuchsia runtime dependency file.

  If the file is in the build directory, then it's stored in /bin, otherwise
  in /assets. This is only a rough heuristic, but is sufficient for the current
  data set.
  """
  norm = os.path.normpath(runtime_dep)
  in_build_dir = not norm.startswith('../')
  no_prefix = norm.strip('/.')
  return ('/bin/' if in_build_dir else '/assets/') + no_prefix


def _RunOnFuchsiaTarget(binary_dir, test, device_name):
  """Runs the given Fuchsia |test| executable on the given |device_name|. The
  device must already be booted.

  Copies the executable and its runtime dependencies as specified by GN to the
  target in /tmp using `netcp`, runs the binary on the target, and logs output
  back to stdout on this machine via `loglistener`.
  """
  arch = 'mac-amd64' if sys.platform == 'darwin' else 'linux-amd64'
  sdk_root = os.path.join(CRASHPAD_DIR, 'third_party', 'fuchsia', 'sdk', arch)

  # Run loglistener and filter the output to know when the test is done.
  loglistener_process = subprocess.Popen(
      [os.path.join(sdk_root, 'tools', 'loglistener'), device_name],
      stdout=subprocess.PIPE, stdin=open(os.devnull), stderr=open(os.devnull))

  runtime_deps_file = os.path.join(binary_dir, test + '.runtime_deps')
  with open(runtime_deps_file, 'rb') as f:
    runtime_deps = f.read().splitlines()

  def netruncmd(*args):
    local_binary = os.path.join(sdk_root, 'tools', 'netruncmd')
    final_args = ' && '.join(' '.join(pipes.quote(x) for x in command)
                             for command in args)
    subprocess.check_call([local_binary, device_name, final_args])

  try:
    tmp_root = '/tmp/tmp_for_%s_%s' % (test, uuid.uuid1().hex)
    staging_root = '/tmp/pkg_for_%s_%s' % (test, uuid.uuid1().hex)

    # Make a staging directory tree on the target.
    directories_to_create = [tmp_root, staging_root, '%s/bin' % staging_root,
                            '%s/assets' % staging_root]
    netruncmd(['mkdir'] + directories_to_create)

    # Copy runtime deps into the staging tree.
    netcp = os.path.join(sdk_root, 'tools', 'netcp')
    for dep in runtime_deps:
      target_path = staging_root + _RuntimeDepsPathToFuchsiaTargetPath(dep)
      subprocess.check_call([netcp, os.path.join(binary_dir, dep),
                            device_name + ':' + target_path],
                            stderr=open(os.devnull))

    done_message = 'TERMINATED: ' + str(uuid.uuid1())
    namespace_command = [
        'namespace', '/pkg=' + staging_root, '/tmp=' + tmp_root, '--',
        staging_root + '/bin/' + test]
    netruncmd(namespace_command, ['echo', done_message])

    success = _HandleOutputFromFuchsiaLogListener(
        loglistener_process, done_message)
    if not success:
      raise subprocess.CalledProcessError(1, test)
  finally:
    netruncmd(['rm', '-rf', tmp_root, staging_root])


# This script is primarily used from the waterfall so that the list of tests
# that are run is maintained in-tree, rather than in a separate infrastructure
# location in the recipe.
def main(args):
  if len(args) != 1:
    print('usage: run_tests.py <binary_dir>', file=sys.stderr)
    return 1

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

  is_fuchsia = _BinaryDirLooksLikeFuchsiaBuild(binary_dir)

  tests = [
      'crashpad_minidump_test',
      'crashpad_test_test',
  ]

  if not is_fuchsia:
    tests.extend([
      # TODO(scottmg): Move the rest of these to the common section once they
      # are building and running successfully.
      'crashpad_client_test',
      'crashpad_handler_test',
      'crashpad_snapshot_test',
      'crashpad_util_test',
      ])

  if is_fuchsia:
    zircon_nodename = os.environ.get('ZIRCON_NODENAME')
    if not zircon_nodename:
      print("Please set ZIRCON_NODENAME to your device's hostname",
            file=sys.stderr)
      return 2
    _GenerateFuchsiaRuntimeDepsFiles(binary_dir, tests)

  for test in tests:
    print('-' * 80)
    print(test)
    print('-' * 80)
    if is_fuchsia:
      _RunOnFuchsiaTarget(binary_dir, test, zircon_nodename)
    else:
      subprocess.check_call(os.path.join(binary_dir, test))

  if sys.platform == 'win32':
    script = 'snapshot/win/end_to_end_test.py'
    print('-' * 80)
    print(script)
    print('-' * 80)
    subprocess.check_call(
        [sys.executable, os.path.join(CRASHPAD_DIR, script), binary_dir])

  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
