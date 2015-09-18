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
import sys

script_dir = os.path.dirname(__file__)
crashpad_dir = os.path.dirname(script_dir) if script_dir is not '' else '..'
sys.path.insert(
    0, os.path.join(crashpad_dir, 'third_party', 'gyp', 'gyp', 'pylib'))

import gyp


def main(args):
  if 'GYP_GENERATORS' not in os.environ:
    os.environ['GYP_GENERATORS'] = 'ninja'

  crashpad_dir_or_dot = crashpad_dir if crashpad_dir is not '' else '.'

  args.extend(['-D', 'crashpad_standalone=1'])
  args.extend(['--include',
               os.path.join(crashpad_dir, 'third_party', 'mini_chromium',
                            'mini_chromium', 'build', 'common.gypi')])
  args.extend(['--depth', crashpad_dir_or_dot])
  args.append(os.path.join(crashpad_dir, 'crashpad.gyp'))

  result = gyp.main(args)
  if result != 0:
    return result

  if sys.platform == 'win32':
    # Also generate the x86 build.
    result = gyp.main(args + ['-D', 'target_arch=ia32', '-G', 'config=Debug'])
    if result != 0:
      return result
    result = gyp.main(args + ['-D', 'target_arch=ia32', '-G', 'config=Release'])

  return result


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
