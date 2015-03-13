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

{
  'includes': [
    '../build/crashpad.gypi',
    '../build/crashpad_in_chromium.gypi',
  ],
  'conditions': [
    ['OS=="mac"', {
      'targets': [
        {
          'target_name': 'crashpad_handler',
          'type': 'executable',
          'dependencies': [
            '../client/client.gyp:crashpad_client',
            '../compat/compat.gyp:crashpad_compat',
            '../minidump/minidump.gyp:crashpad_minidump',
            '../snapshot/snapshot.gyp:crashpad_snapshot',
            '../third_party/mini_chromium/mini_chromium.gyp:base',
            '../tools/tools.gyp:crashpad_tool_support',
            '../util/util.gyp:crashpad_util',
          ],
          'include_dirs': [
            '..',
          ],
          'sources': [
            'mac/crash_report_exception_handler.cc',
            'mac/crash_report_exception_handler.h',
            'mac/crash_report_upload_thread.cc',
            'mac/crash_report_upload_thread.h',
            'mac/exception_handler_server.cc',
            'mac/exception_handler_server.h',
            'mac/main.cc',
          ],

	  # In an in-Chromium build with component=shared_library,
	  # crashpad_handler will depend on shared libraries such as
	  # libbase.dylib located in out/{Debug,Release} via the @rpath
	  # mechanism. When crashpad_handler is copied to its home deep inside
	  # the Chromium app bundle, it needs to have an LC_RPATH command
	  # pointing back to the directory containing these dependency
	  # libraries.
          'variables': {
            'component%': 'static_library',
          },
          'conditions': [
            ['crashpad_in_chromium!=0 and component=="shared_library"', {
              'xcode_settings': {
                'LD_RUNPATH_SEARCH_PATHS': [  # -Wl,-rpath
                  # Get back from
                  # Chromium.app/Contents/Versions/V/Framework.framework/Helpers
                  '@loader_path/../../../../../..',
                ],
              },
            }],
          ],
        },
      ],
    }, {
      'targets': [],
    }],
  ],
}
