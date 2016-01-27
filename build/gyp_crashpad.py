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


DEPENDENCIES_LOCAL = 0
DEPENDENCIES_CHROMIUM = 1
DEPENDENCIES_EXTERNAL = 2

# Chooses between a local path and an external path, preferring the local path.
# If the local path is not present but the external path is, returns the
# external path. If neither path is present, returns the local path, so that
# error messages uniformly refer to the local path.
#
# The return value is a 2-tuple. The first element is DEPENDENCIES_LOCAL or
# DEPENDENCIES_EXTERNAL, and the second element is the path. This will never
# return DEPENDENCIES_CHROMIUM, as that mode is chosen by a different mechanism.
# See build/crashpad_dependencies.gypi.
def ChoosePath(local_path, external_path):
  if os.path.exists(local_path) or not os.path.exists(external_path):
    return (LOCAL, local_path)
  return (EXTERNAL, external_path)


script_dir = os.path.dirname(__file__)
crashpad_dir = (os.path.dirname(script_dir) if script_dir not in ('', os.curdir)
                else os.pardir)

sys.path.insert(0,
    ChoosePath(os.path.join(crashpad_dir, 'third_party', 'gyp', 'gyp', 'pylib'),
               os.path.join(crashpad_dir, os.pardir, 'gyp', 'pylib'))[1])

import gyp


def main(args):
  if 'GYP_GENERATORS' not in os.environ:
    os.environ['GYP_GENERATORS'] = 'ninja'

  crashpad_dir_or_dot = crashpad_dir if crashpad_dir is not '' else os.curdir

  (dependencies, mini_chromium_dir) = (
      ChoosePath(os.path.join(crashpad_dir, 'third_party', 'mini_chromium',
                              'mini_chromium', 'build', 'common.gypi'),
                 os.path.join(crashpad_dir, os.pardir, 'mini_chromium', 'build',
                              'common.gypi')))
  args.extend(['-D', 'crashpad_dependencies=%d' % dependencies])
  args.extend(['--include', mini_chromium_dir])
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
