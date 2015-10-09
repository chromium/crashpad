#!/usr/bin/env python

# Copyright 2015 The Crashpad Authors. All rights reserved.
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


def FindInstalledWindowsApplication(app_path):
  search_paths = [os.getenv('PROGRAMFILES(X86)'),
                  os.getenv('PROGRAMFILES'),
                  os.getenv('LOCALAPPDATA')]
  search_paths += os.getenv('PATH', '').split(os.pathsep)

  for search_path in search_paths:
    if not search_path:
      continue
    path = os.path.join(search_path, app_path)
    if os.path.isfile(path):
      return path

  return None


def GetCdbPath():
  possible_paths = (
      os.path.join('Windows Kits', '10', 'Debuggers', 'x64'),
      os.path.join('Windows Kits', '10', 'Debuggers', 'x86'),
      os.path.join('Windows Kits', '8.1', 'Debuggers', 'x64'),
      os.path.join('Windows Kits', '8.1', 'Debuggers', 'x86'),
      os.path.join('Windows Kits', '8.0', 'Debuggers', 'x64'),
      os.path.join('Windows Kits', '8.0', 'Debuggers', 'x86'),
      'Debugging Tools For Windows (x64)',
      'Debugging Tools For Windows (x86)',
      'Debugging Tools For Windows',
      os.path.join('win_toolchain', 'vs2013_files', 'win8sdk', 'Debuggers',
                    'x64'),
      os.path.join('win_toolchain', 'vs2013_files', 'win8sdk', 'Debuggers',
                    'x86'),
  )
  for possible_path in possible_paths:
    app_path = os.path.join(possible_path, 'cdb.exe')
    app_path = FindInstalledWindowsApplication(app_path)
    if app_path:
      return app_path
  return None


def main(args):
  cdb_path = GetCdbPath()
  print 'cdb_path:', cdb_path
  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
