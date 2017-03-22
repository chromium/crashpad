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
import glob
import gyp_crashpad
import os
import subprocess
import sys


def main(args):
  parser = argparse.ArgumentParser(
      description='Set up an Android cross build',
      epilog='Additional arguments will be passed to gyp_crashpad.py.')
  parser.add_argument('--ndk', required=True, help='Standalone NDK toolchain')
  parser.add_argument('--compiler',
                      default='clang',
                      choices=('clang', 'gcc'),
                      help='The compiler to use, clang by default')
  (parsed, extra_args) = parser.parse_known_args(args)

  NDK_ERROR=(
      'NDK must be a valid standalone NDK toolchain.\n' +
      'See https://developer.android.com/ndk/guides/standalone_toolchain.html')
  arch_dirs = glob.glob(os.path.join(parsed.ndk, '*-linux-android*'))
  if len(arch_dirs) != 1:
    parser.error(NDK_ERROR)

  arch_triplet = os.path.basename(arch_dirs[0])
  ARCH_TRIPLET_TO_ARCH = {
    'arm-linux-androideabi': 'arm',
    'aarch64-linux-android': 'arm64',
    'i686-linux-android': 'x86',
    'x86_64-linux-android': 'x86_64',
    'mipsel-linux-android': 'mips',
    'mips64el-linux-android': 'mips64',
  }
  if arch_triplet not in ARCH_TRIPLET_TO_ARCH:
    parser.error(NDK_ERROR)
  arch = ARCH_TRIPLET_TO_ARCH[arch_triplet]

  ndk_bin_dir = os.path.join(parsed.ndk, 'bin')

  if parsed.compiler == 'clang':
    os.environ['CC_target'] = os.path.join(ndk_bin_dir, 'clang')
    os.environ['CXX_target'] = os.path.join(ndk_bin_dir, 'clang++')
  elif parsed.compiler == 'gcc':
    os.environ['CC_target'] = os.path.join(ndk_bin_dir,
                                           '%s-gcc' % arch_triplet)
    os.environ['CXX_target'] = os.path.join(ndk_bin_dir,
                                            '%s-g++' % arch_triplet)

  for tool in ('ar', 'nm', 'readelf'):
    os.environ['%s_target' % tool.upper()] = (
        os.path.join(ndk_bin_dir, '%s-%s' % (arch_triplet, tool)))

  return gyp_crashpad.main(
      ['-D', 'OS=android',
       '-D', 'target_arch=%s' % arch,
       '-D', 'clang=%d' % (1 if parsed.compiler == 'clang' else 0),
       '-f', 'ninja-android'] + extra_args)


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
