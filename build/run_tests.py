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
IS_WINDOWS_HOST = sys.platform.startswith('win')


def _GetFuchsiaSDKRoot():
  arch = 'mac-amd64' if sys.platform == 'darwin' else 'linux-amd64'
  return os.path.join(CRASHPAD_DIR, 'third_party', 'fuchsia', 'sdk', arch)


def _BinaryDirLooksLikeFuchsiaBuild(binary_dir):
  """Checks whether the provided output directory targets Fuchsia."""
  popen = subprocess.Popen(
      ['gn', 'args', binary_dir, '--list=target_os', '--short'],
      shell=IS_WINDOWS_HOST, stdout=subprocess.PIPE, stderr=open(os.devnull))
  value = popen.communicate()[0]
  return popen.returncode == 0 and 'target_os = "fuchsia"' in value


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


def _RunOnFuchsiaTarget(binary_dir, test, device_name):
  """Runs the given Fuchsia |test| executable on the given |device_name|. The
  device must already be booted.

  Copies the executable and its runtime dependencies as specified by GN to the
  target in /tmp using `netcp`, runs the binary on the target, and logs output
  back to stdout on this machine via `loglistener`.
  """
  sdk_root = _GetFuchsiaSDKRoot()

  # Run loglistener and filter the output to know when the test is done.
  loglistener_process = subprocess.Popen(
      [os.path.join(sdk_root, 'tools', 'loglistener'), device_name],
      stdout=subprocess.PIPE, stdin=open(os.devnull), stderr=open(os.devnull))

  runtime_deps_file = os.path.join(binary_dir, test + '.runtime_deps')
  with open(runtime_deps_file, 'rb') as f:
    runtime_deps = f.read().splitlines()

  def netruncmd(*args):
    """Runs a list of commands on the target device. Each command is escaped
    by using pipes.quote(), and then each command is chained by shell ';'.
    """
    netruncmd_path = os.path.join(sdk_root, 'tools', 'netruncmd')
    final_args = ' ; '.join(' '.join(pipes.quote(x) for x in command)
                            for command in args)
    subprocess.check_call([netruncmd_path, device_name, final_args])

  try:
    unique_id = uuid.uuid4().hex
    tmp_root = '/tmp/%s_%s/tmp' % (test, unique_id)
    staging_root = '/tmp/%s_%s/pkg' % (test, unique_id)

    # Make a staging directory tree on the target.
    directories_to_create = [tmp_root, '%s/bin' % staging_root,
                             '%s/assets' % staging_root]
    netruncmd(['mkdir', '-p'] + directories_to_create)

    def netcp(local_path):
      """Uses `netcp` to copy a file or directory to the device. Files located
      inside the build dir are stored to /pkg/bin, otherwise to /pkg/assets.
      """
      in_binary_dir = local_path.startswith(binary_dir + '/')
      if in_binary_dir:
        target_path = os.path.join(
            staging_root, 'bin', local_path[len(binary_dir)+1:])
      else:
        target_path = os.path.join(staging_root, 'assets', local_path)
      netcp_path = os.path.join(sdk_root, 'tools', 'netcp')
      subprocess.check_call([netcp_path, local_path,
                             device_name + ':' + target_path],
                            stderr=open(os.devnull))

    # Copy runtime deps into the staging tree.
    for dep in runtime_deps:
      local_path = os.path.normpath(os.path.join(binary_dir, dep))
      if os.path.isdir(local_path):
        for root, dirs, files in os.walk(local_path):
          for f in files:
            netcp(os.path.join(root, f))
      else:
        netcp(local_path)

    done_message = 'TERMINATED: ' + unique_id
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
  if len(args) != 1 and len(args) != 2:
    print('usage: run_tests.py <binary_dir> [test_to_run]', file=sys.stderr)
    return 1

  binary_dir = args[0]
  single_test = args[1] if len(args) == 2 else None

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
      'crashpad_snapshot_test',
      'crashpad_test_test',
      'crashpad_util_test',
  ]

  if not is_fuchsia:
    tests.extend([
      # TODO(scottmg): Move the rest of these to the common section once they
      # are building and running successfully.
      'crashpad_client_test',
      'crashpad_handler_test',
      ])

  if is_fuchsia:
    zircon_nodename = os.environ.get('ZIRCON_NODENAME')
    if not zircon_nodename:
      netls = os.path.join(_GetFuchsiaSDKRoot(), 'tools', 'netls')
      popen = subprocess.Popen([netls, '--nowait'], stdout=subprocess.PIPE)
      devices = popen.communicate()[0].splitlines()
      if popen.returncode != 0 or len(devices) != 1:
        print("Please set ZIRCON_NODENAME to your device's hostname",
              file=sys.stderr)
        return 2
      zircon_nodename = devices[0].strip().split()[1]
      print('Using autodetected Fuchsia device:', zircon_nodename)
    _GenerateFuchsiaRuntimeDepsFiles(
        binary_dir, [t for t in tests if not t.endswith('.py')])
  elif IS_WINDOWS_HOST:
    tests.append('snapshot/win/end_to_end_test.py')

  if single_test:
    if single_test not in tests:
      print('Unrecognized test:', single_test, file=sys.stderr)
      return 3
    tests = [single_test]

  for test in tests:
    if test.endswith('.py'):
      print('-' * 80)
      print(test)
      print('-' * 80)
      subprocess.check_call(
          [sys.executable, os.path.join(CRASHPAD_DIR, test), binary_dir])
    else:
      if is_fuchsia:
        _RunOnFuchsiaTarget(binary_dir, test, zircon_nodename)
      else:
        subprocess.check_call(os.path.join(binary_dir, test))

  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
