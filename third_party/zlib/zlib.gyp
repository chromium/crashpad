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

{
  'variables': {
    'conditions': [
      ['OS!="win"', {
        'embedded_zlib': 0,
      }, {
        'embedded_zlib': 1,
      }],
    ],
  },
  'targets': [
    {
      'target_name': 'zlib',
      'conditions': [
        ['<(embedded_zlib)==0', {
          'type': 'none',
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/usr/lib/libz.dylib',
            ],
          },
        }, {
          'type': 'static_library',
          'include_dirs': [
              'zlib',
          ],
          'defines': [
            'CRASHPAD_EMBEDDED_ZLIB',
            'HAVE_STDARG_H',
          ],
          'direct_dependent_settings': {
            'include_dirs': [
              'zlib',
            ],
            'defines': [
              'CRASHPAD_EMBEDDED_ZLIB',
            ],
          },
          'sources': [
            'zlib.h',
            'zlib/adler32.c',
            'zlib/compress.c',
            'zlib/crc32.c',
            'zlib/crc32.h',
            'zlib/deflate.c',
            'zlib/deflate.h',
            'zlib/gzclose.c',
            'zlib/gzguts.h',
            'zlib/gzlib.c',
            'zlib/gzread.c',
            'zlib/gzwrite.c',
            'zlib/infback.c',
            'zlib/inffast.c',
            'zlib/inffast.h',
            'zlib/inffixed.h',
            'zlib/inflate.c',
            'zlib/inflate.h',
            'zlib/inftrees.c',
            'zlib/inftrees.h',
            'zlib/trees.c',
            'zlib/trees.h',
            'zlib/uncompr.c',
            'zlib/zconf.h',
            'zlib/zlib.h',
            'zlib/zutil.c',
            'zlib/zutil.h',
          ],
          'conditions': [
            ['OS!="win"', {
              'defines': [
                'HAVE_HIDDEN',
                'HAVE_UNISTD_H',
              ],
            }, {
              'msvs_disabled_warnings': [
                4131,  # uses old-style declarator
                4244,  # conversion from 't1' to 't2', possible loss of data
                4245,  # conversion from 't1' to 't2', signed/unsigned mismatch
                4267,  # conversion from 'size_t' to 't', possible loss of data
              ],
            }],
          ],
        }],
      ],
    },
  ],
}
