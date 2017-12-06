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

import os
import re
import subprocess
import sys


def parse_version(version_str):
  """'10.6' => [10, 6]"""
  return map(int, re.findall(r'(\d+)', version_str))


def main():
  if len(sys.argv) != 2:
    print 'usage: find_mac_sdk.py min_sdk_version'
    return 1
  min_sdk_version = sys.argv[1]

  out = subprocess.check_output(['xcode-select', '-print-path'])
  sdk_dir = os.path.join(
      out.rstrip(), 'Platforms/MacOSX.platform/Developer/SDKs')
  sdks = [re.findall('^MacOSX(10\.\d+)\.sdk$', s) for s in os.listdir(sdk_dir)]
  sdks = [s[0] for s in sdks if s]  # [['10.5'], ['10.6']] => ['10.5', '10.6']
  sdks = [s for s in sdks  # ['10.5', '10.6'] => ['10.6']
          if parse_version(s) >= parse_version(min_sdk_version)]
  best_sdk = sorted(sdks, key=parse_version)[0]
  print os.path.join(sdk_dir, 'MacOSX' + best_sdk + '.sdk')


if __name__ == '__main__':
  main()
