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

import argparse
import os
import subprocess
import sys

def main(argv):
  this_dir = os.path.dirname(__file__)
  swarming_client_dir = os.path.join(this_dir, os.pardir, 'third_party',
                                     'swarming_client')
  parser = argparse.ArgumentParser()
  parser.add_argument('isolate_file')
  args, remaining_args = parser.parse_known_args()
  out = subprocess.check_output([
    sys.executable,
    os.path.join(swarming_client_dir, 'isolate.py'),
    'archive',
    '--isolate-server', 'isolateserver.appspot.com',
    '--isolate', args.isolate_file,
    '--isolated', args.isolate_file + 'd',
  ])
  isolated_hash, taskname = out.split()

  return subprocess.call([
    sys.executable,
    os.path.join(swarming_client_dir, 'swarming.py'),
    'run',
    '--isolated', isolated_hash,
    '--isolate-server', 'isolateserver.appspot.com',
    '--swarming', 'chromium-swarm.appspot.com',
  ] + remaining_args)


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
