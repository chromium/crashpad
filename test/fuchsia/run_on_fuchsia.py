#!/usr/bin/env python
# coding: utf-8

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

from __future__ import print_function

import os
import pipes
import random
import re
import struct
import subprocess
import sys


def main(args):
  zircon_tools_path = os.environ.get('ZIRCON_TOOLS')
  loglistener_path = os.path.join(zircon_tools_path, 'loglistener')
  netruncmd_path = os.path.join(zircon_tools_path, 'netruncmd')

  zircon_nodename = os.environ.get('ZIRCON_NODENAME')

  run_and_log_path = os.environ.get('FUCHSIA_RUN_AND_LOG')

  prefix = ''.join(chr(random.randint(ord('!'),
                                      ord('~'))) for x in range(0, 16))

  args.insert(0, run_and_log_path)
  args.insert(1, '--prefix=' + prefix)
  args.insert(2, '--')
  args_string = ' '.join(pipes.quote(x) for x in args)
  print(args_string)

  prefix_re = re.compile(b'\\[-?\d{5,}\.-?\d{3}] \d{5,}\.\d{5,}> ' +
                         re.escape(prefix).encode('utf-8') +
                         b'(.) (.*)$')

  loglistener_process = subprocess.Popen([loglistener_path, zircon_nodename],
                                         stdout=subprocess.PIPE,
                                         stdin=open(os.devnull))
  try:
    subprocess.check_call([netruncmd_path, zircon_nodename, args_string])

    while True:
      line = loglistener_process.stdout.readline().rstrip(b'\r\n')
      match = prefix_re.match(line)
      if match:
        channel = match.group(1)
        out_file = None
        if channel == b'1':
          out_file = sys.stdout
        elif channel == b'2':
          out_file = sys.stderr
        elif channel == b'5':
          status = int(match.group(2))
          break

        if out_file:
          in_string = match.group(2)
          i = 0
          eol_no_newline = False
          out_string = b''
          while i < len(in_string):
            c = in_string[i:i+1]
            if c == b'\\':
              if i == len(in_string) - 1:
                eol_no_newline = True
              else:
                i += 1
                ec = in_string[i:i+1]
                if ec == b'a':
                  c = b'\a'
                elif ec == b'b':
                  c = b'\b'
                elif ec == b'f':
                  c = b'\f'
                elif ec == b'n':
                  c = b'\n'
                elif ec == b'r':
                  c = b'\r'
                elif ec == b't':
                  c = b'\t'
                elif ec == b'v':
                  c = b'\v'
                elif ec == b'\\':
                  pass  # c is already b'\\'
                elif ec == b'x':
                  c = struct.pack('B', int(in_string[i+1:i+3], 16))
                  i += 2
            if not eol_no_newline:
              out_string += c
            i += 1

            if i > len(in_string):
              # Invalid escape sequence
              raise IndexError('index out of range')

          if not eol_no_newline:
            out_string += b'\n'

          if hasattr(out_file, 'buffer'):
            out_file = out_file.buffer

          out_file.write(out_string)
  finally:
    loglistener_process.terminate()
    loglistener_process.wait()

  return status


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
