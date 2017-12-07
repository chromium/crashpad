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
import posixpath
import re
import subprocess
import sys
import uuid

CRASHPAD_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                            os.pardir)
IS_WINDOWS_HOST = sys.platform.startswith('win')


def _BinaryDirTargetOS(binary_dir):
  """Returns the apparent target OS of binary_dir, or None if none appear to be
  explicitly specified."""

  # Look for a GN “target_os”.
  popen = subprocess.Popen(
      ['gn', 'args', binary_dir, '--list=target_os', '--short'],
      shell=IS_WINDOWS_HOST, stdout=subprocess.PIPE, stderr=open(os.devnull))
  value = popen.communicate()[0]
  if popen.returncode == 0:
    match = re.match('target_os = "(.*)"$', value.decode('utf-8'))
    if match:
      return match.group(1)

  # For GYP with Ninja, look for the appearance of “linux-android” in the path
  # to ar. This path is configured by gyp_crashpad_android.py.
  try:
    with open(os.path.join(binary_dir, 'build.ninja')) as build_ninja_file:
      build_ninja_content = build_ninja_file.read()
      match = re.search('^ar = .+-linux-android(eabi)?-ar$',
                        build_ninja_content,
                        re.MULTILINE)
      if match:
        return 'android'
  except FileNotFoundError:
    # Ninja may not be in use. Assume the best.
    pass

  return None


def _RunOnAndroidTarget(binary_dir, test, android_device):
  local_test_path = os.path.join(binary_dir, test)
  MAYBE_UNSUPPORTED_TESTS = (
      'crashpad_client_test',
      'crashpad_handler_test',
      'crashpad_minidump_test',
      'crashpad_snapshot_test',
  )
  if not os.path.exists(local_test_path) and test in MAYBE_UNSUPPORTED_TESTS:
    print(test, 'is not present and may not be supported, skipping')
    return

  device_temp_dir = subprocess.check_output(
      ['adb', '-s', android_device, 'shell',
       'mktemp', '-d', '/data/local/tmp/%s.XXXXXXXX' % test],
      shell=IS_WINDOWS_HOST).decode('utf-8').rstrip()
  try:
    # Specify test dependencies that must be pushed to the device. This could be
    # determined automatically in a GN build, following the example used for
    # Fuchsia. Since nothing like that exists for GYP, hard-code it for
    # supported tests.
    test_build_artifacts = [test]
    test_data = ['test/test_paths_test_data_root.txt']

    if test == 'crashpad_test_test':
      test_build_artifacts.append(
          'crashpad_test_test_multiprocess_exec_test_child')
    elif test == 'crashpad_util_test':
      test_data.append('util/net/testdata/')

    def _adb(*args):
      # Flush all of this script’s own buffered stdout output before running
      # adb, which will likely produce its own output on stdout.
      sys.stdout.flush()

      adb_command = ['adb', '-s', android_device]
      adb_command.extend(args)
      subprocess.check_call(adb_command, shell=IS_WINDOWS_HOST)

    # Establish the directory structure on the device.
    device_out_dir = posixpath.join(device_temp_dir, 'out')
    device_mkdirs = [device_out_dir]
    for source_path in test_data:
      # A trailing slash could reasonably mean to copy an entire directory, but
      # will interfere with what’s needed from the path split. All parent
      # directories of any source_path need to be be represented in
      # device_mkdirs, but it’s important that no source_path itself wind up in
      # device_mkdirs, even if source_path names a directory, because that would
      # cause the “adb push” of the directory below to behave incorrectly.
      if source_path.endswith(posixpath.sep):
        source_path = source_path[:-1]

      device_source_path = posixpath.join(device_temp_dir, source_path)
      device_mkdir = posixpath.split(device_source_path)[0]
      if device_mkdir not in device_mkdirs:
        device_mkdirs.append(device_mkdir)
    adb_mkdir_command = ['shell', 'mkdir', '-p']
    adb_mkdir_command.extend(device_mkdirs)
    _adb(*adb_mkdir_command)

    # Push the test binary and any other build output to the device.
    adb_push_command = ['push']
    for artifact in test_build_artifacts:
      adb_push_command.append(os.path.join(binary_dir, artifact))
    adb_push_command.append(device_out_dir)
    _adb(*adb_push_command)

    # Push test data to the device.
    for source_path in test_data:
      _adb('push',
           os.path.join(CRASHPAD_DIR, source_path),
           posixpath.join(device_temp_dir, source_path))

    # Run the test on the device.
    _adb('shell', 'env', 'CRASHPAD_TEST_DATA_ROOT=' + device_temp_dir,
         posixpath.join(device_out_dir, test))
  finally:
    _adb('shell', 'rm', '-rf', device_temp_dir)


def _GetFuchsiaSDKRoot():
  arch = 'mac-amd64' if sys.platform == 'darwin' else 'linux-amd64'
  return os.path.join(CRASHPAD_DIR, 'third_party', 'fuchsia', 'sdk', arch)


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

  target_os = _BinaryDirTargetOS(binary_dir)
  is_android = target_os == 'android'
  is_fuchsia = target_os == 'fuchsia'

  tests = [
      'crashpad_client_test',
      'crashpad_handler_test',
      'crashpad_minidump_test',
      'crashpad_snapshot_test',
      'crashpad_test_test',
      'crashpad_util_test',
  ]

  if is_android:
    android_device = os.environ.get('ANDROID_DEVICE')
    if not android_device:
      adb_devices = subprocess.check_output(['adb', 'devices'],
                                            shell=IS_WINDOWS_HOST)
      devices = []
      for line in adb_devices.splitlines():
        line = line.decode('utf-8')
        if (line == 'List of devices attached' or
            re.match('^\* daemon .+ \*$', line) or
            line == ''):
          continue
        (device, ignore) = line.split('\t')
        devices.append(device)
      if len(devices) != 1:
        print("Please set ANDROID_DEVICE to your device's id", file=sys.stderr)
        return 2
      android_device = devices[0]
      print('Using autodetected Android device:', android_device)
  elif is_fuchsia:
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
      if is_android:
        _RunOnAndroidTarget(binary_dir, test, android_device)
      elif is_fuchsia:
        _RunOnFuchsiaTarget(binary_dir, test, zircon_nodename)
      else:
        subprocess.check_call(os.path.join(binary_dir, test))

  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
