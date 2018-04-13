#!/usr/bin/env python

# Copyright 2018 The Crashpad Authors. All rights reserved.
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

# Minimal script for a Go app.

import argparse
import os
import subprocess
import sys


SCRIPT_DIR = os.path.abspath(os.path.dirname(__file__))


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--root-out-dir', help='Path to root of build output',
                      required=True)
  parser.add_argument('--current-cpu', help='Target architecture.',
                      choices=['x86', 'x64', 'arm64'], required=True)
  parser.add_argument('--current-os', help='Target operating system.',
                      choices=['fuchsia', 'linux', 'mac', 'win'], required=True)
  parser.add_argument('--binname', help='Output file', required=True)
  parser.add_argument('sources', metavar='SRC', type=str, nargs='+')
  args = parser.parse_args()

  goarch = {
    'x86': '386',
    'x64': 'amd64',
    'arm64': 'arm64',
  }[args.current_cpu]
  goos = {
    'fuchsia': 'fuchsia',
    'linux': 'linux',
    'mac': 'darwin',
    'win': 'windows',
  }[args.current_os]

  host = {
    'darwin': 'mac',
    'win32': 'windows',
    'linux2': 'linux',
  }[sys.platform]

  output_name = os.path.join(args.root_out_dir, args.binname)

  env = {}
  toolchain = os.path.join(SCRIPT_DIR, os.pardir,
                           'third_party', 'go', host + '-amd64')
  env['PATH'] = toolchain + os.pathsep + os.environ.get('PATH')
  env['GOARCH'] = goarch
  env['GOOS'] = goos

  cmd = ['go', 'build', '-o', output_name] + args.sources
  print 'RUNNING', cmd
  subprocess.check_call(cmd, env=env)


if __name__ == '__main__':
  sys.exit(main())
