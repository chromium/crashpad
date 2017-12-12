#!/usr/bin/env python

# Copyright 2017 The Crashpad Authors. All rights reserved.
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

"""Helper script to [re]start or stop a helper Fuchsia QEMU instance to be used
for running tests without a device.
"""

from __future__ import print_function

import os
import signal
import subprocess
import sys


CRASHPAD_ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                             os.pardir)


def _Stop(pid_file):
  if os.path.isfile(pid_file):
    with open(pid_file, 'rb') as f:
      pid = int(f.read())
      try:
        os.kill(pid, signal.SIGTERM)
      except:
        print('Unable to kill pid %d, continuing' % pid, file=sys.stderr)
    os.unlink(pid_file)


def _Start(pid_file):
  arch = 'mac-amd64' if sys.platform == 'darwin' else 'linux-amd64'
  qemu_path = os.path.join(CRASHPAD_ROOT, 'third_party', 'fuchsia', 'qemu',
                           arch, 'bin', 'qemu-system-x86_64')
  kernel_path = os.path.join(CRASHPAD_ROOT, 'third_party', 'fuchsia', 'sdk',
                             arch, 'target', 'x86_64', 'zircon.bin')
  initrd_path = os.path.join(CRASHPAD_ROOT, 'third_party', 'fuchsia', 'sdk',
                             arch, 'target', 'x86_64', 'bootdata.bin')

  # These arguments are from the Fuchsia repo in zircon/scripts/run-zircon.
  popen = subprocess.Popen([
    qemu_path,
    '-m', '2048',
    '-nographic',
    '-kernel', kernel_path,
    '-initrd', initrd_path,
    '-smp', '4',
    '-serial', 'stdio',
    '-monitor', 'none',
    '-machine', 'q35',
    '-cpu', 'host,migratable=no',
    '-enable-kvm',
    '-netdev', 'type=tap,ifname=qemu,script=no,downscript=no,id=net0',
    '-device', 'e1000,netdev=net0,mac=52:54:00:63:5e:7b',
    '-append', 'TERM=dumb',
  ])

  with open(pid_file, 'wb') as f:
    f.write('%d' % popen.pid)


def main(args):
  if len(args) != 1 or (args[0] != 'start' and args[0] != 'stop'):
    print('usage: run_fuchsia_qemu.py start|stop', file=sys.stderr)
    return 1

  command = args[0]

  pid_file = os.path.join(CRASHPAD_ROOT, '.fuchsia_qemu_pid')
  _Stop(pid_file)
  if command == 'start':
    _Start(pid_file)

  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
