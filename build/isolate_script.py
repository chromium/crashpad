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

import json
import os
import pipes
import pprint
import stat
import sys


def write_files(crashpad_dir, binary_dir, target, command, files):
  """Writes the files needed to run an isolated script.

  Given a binary_dir inside the crashpad_root, a list of files either relative
  to the binary_dir or src-relative to the crashpad_dir, the name for a target,
  and an array containing a path to a script and any additional arguments,
  this writes a wrapper script in the binary_dir that will invoke the python
  script, and the needed files to generate an isolate for that command."""

  command[0] = _rewrite_source_abspath(command[0], crashpad_dir, binary_dir)
  binary_dir_relpath = os.path.relpath(binary_dir, crashpad_dir)

  _write(os.path.join(binary_dir, '%s.isolate' % target),
         pprint.pformat({
             'variables': {
               'command': command,
               'files': [_rewrite_source_abspath(f, crashpad_dir, binary_dir)
                         for f in files],
             }}) + '\n')

  _write(os.path.join(binary_dir, '%s.isolated.gen.json' % target),
         json.dumps({
             'args': [
                 '--isolated',
                 '//%s/%s.isolated' % (binary_dir_relpath, target),
                 '--isolate',
                 '//%s/%s.isolate' % (binary_dir_relpath, target)],
             'dir': crashpad_dir,
             'version': 1,
             }))

  if sys.platform == 'win32':
    _write(os.path.join(binary_dir, target + '.bat'),
           '@%s %s\n' % (sys.executable, ' '.join('"%s"' % c for c in command)))
    _make_executable(os.path.join(binary_dir, target + '.bat'))
  else:
    _write(os.path.join(binary_dir, target),
           '#!/bin/bash\ncd $(dirname "$0")\n%s %s\n' % (sys.executable,
                                   ' '.join(pipes.quote(c) for c in command)))
    _make_executable(os.path.join(binary_dir, target))

  return 0


def _write(path, contents):
  with open(path, 'w') as fp:
    fp.write(contents)


def _make_executable(path):
  os.chmod(path,
           (stat.S_IRUSR | stat.S_IWUSR | stat.S_IXUSR |
            stat.S_IRGRP | stat.S_IWGRP | stat.S_IXGRP |
            stat.S_IROTH | stat.S_IXOTH))


def _rewrite_source_abspath(path, crashpad_dir, binary_dir):
  if path.startswith('//'):
    return '%s/%s' % (os.path.relpath(crashpad_dir, binary_dir), path[2:])
  else:
    return path
