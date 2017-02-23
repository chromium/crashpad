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
  ],
  'targets': [
    {
      # This target exists so that the crashpad_handler can be embedded into
      # another binary.
      'target_name': 'crashpad_handler_lib',
      'type': 'static_library',
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
        'crash_report_upload_thread.cc',
        'crash_report_upload_thread.h',
        'handler_main.cc',
        'handler_main.h',
        'mac/crash_report_exception_handler.cc',
        'mac/crash_report_exception_handler.h',
        'mac/exception_handler_server.cc',
        'mac/exception_handler_server.h',
        'prune_crash_reports_thread.cc',
        'prune_crash_reports_thread.h',
        'win/crash_report_exception_handler.cc',
        'win/crash_report_exception_handler.h',
      ],
    },
    {
      'target_name': 'crashpad_handler',
      'type': 'executable',
      'dependencies': [
        '../third_party/mini_chromium/mini_chromium.gyp:base',
        '../tools/tools.gyp:crashpad_tool_support',
        'crashpad_handler_lib',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'main.cc',
      ],

      'conditions': [
        ['OS=="win"',  {
          'msvs_settings': {
            'VCLinkerTool': {
              'SubSystem': '2',  # /SUBSYSTEM:WINDOWS
            },
          },
        }],
      ],
    },
  ],
  'conditions': [
    ['OS=="win"', {
      'targets': [
        {
          # Duplicates crashpad_handler.exe to crashpad_handler.com and makes it
          # a console app.
          'target_name': 'crashpad_handler_console',
          'type': 'none',
          'dependencies': [
            '../third_party/mini_chromium/mini_chromium.gyp:base',
            '../tools/tools.gyp:crashpad_tool_support',
            'crashpad_handler',
          ],
          'actions': [
            {
              'action_name': 'copy handler exe to com',
              'inputs': [
                '<(PRODUCT_DIR)/crashpad_handler.exe',
              ],
              'outputs': [
                '<(PRODUCT_DIR)/crashpad_handler.com',
              ],
              'action': [
                'copy <(PRODUCT_DIR)\crashpad_handler.exe '
                    '<(PRODUCT_DIR)\crashpad_handler.com >nul && '
                'editbin -nologo -subsystem:console '
                    '<(PRODUCT_DIR)\crashpad_handler.com >nul',
              ],
              'msvs_cygwin_shell': '0',
              'quote_cmd': '0',
            },
          ],
        },
        {
          'target_name': 'crashy_program',
          'type': 'executable',
          'dependencies': [
            '../client/client.gyp:crashpad_client',
            '../third_party/mini_chromium/mini_chromium.gyp:base',
            '../util/util.gyp:crashpad_util',
          ],
          'include_dirs': [
            '..',
          ],
          'sources': [
            'win/crashy_test_program.cc',
          ],
        },
        {
          'target_name': 'crashy_signal',
          'type': 'executable',
          'dependencies': [
            '../client/client.gyp:crashpad_client',
            '../third_party/mini_chromium/mini_chromium.gyp:base',
          ],
          'include_dirs': [
            '..',
          ],
          'sources': [
            'win/crashy_signal.cc',
          ],
        },
        {
          'target_name': 'crash_other_program',
          'type': 'executable',
          'dependencies': [
            '../client/client.gyp:crashpad_client',
            '../test/test.gyp:crashpad_test',
            '../third_party/mini_chromium/mini_chromium.gyp:base',
            '../util/util.gyp:crashpad_util',
          ],
          'sources': [
            'win/crash_other_program.cc',
          ],
        },
        {
          'target_name': 'fake_handler_that_crashes_at_startup',
          'type': 'executable',
          'sources': [
            'win/fake_handler_that_crashes_at_startup.cc',
          ],
        },
        {
          'target_name': 'hanging_program',
          'type': 'executable',
          'dependencies': [
            '../client/client.gyp:crashpad_client',
            '../third_party/mini_chromium/mini_chromium.gyp:base',
          ],
          'sources': [
            'win/hanging_program.cc',
          ],
        },
        {
          'target_name': 'loader_lock_dll',
          'type': 'loadable_module',
          'sources': [
            'win/loader_lock_dll.cc',
          ],
          'msvs_settings': {
            'NoImportLibrary': 'true',
          },
        },
        {
          'target_name': 'self_destroying_program',
          'type': 'executable',
          'dependencies': [
            '../client/client.gyp:crashpad_client',
            '../compat/compat.gyp:crashpad_compat',
            '../snapshot/snapshot.gyp:crashpad_snapshot',
            '../third_party/mini_chromium/mini_chromium.gyp:base',
            '../util/util.gyp:crashpad_util',
          ],
          'include_dirs': [
            '..',
          ],
          'sources': [
            'win/self_destroying_test_program.cc',
          ],
        },
      ],
      'conditions': [
        # Cannot create an x64 DLL with embedded debug info.
        ['target_arch=="ia32"', {
          'targets': [
            {
              'target_name': 'crashy_z7_loader',
              'type': 'executable',
              'dependencies': [
                '../client/client.gyp:crashpad_client',
                '../test/test.gyp:crashpad_test',
                '../third_party/mini_chromium/mini_chromium.gyp:base',
              ],
              'include_dirs': [
                '..',
              ],
              'sources': [
                'win/crashy_test_z7_loader.cc',
              ],
            },
          ],
        }],
      ],
    }, {
      'targets': [],
    }],
  ],
}
