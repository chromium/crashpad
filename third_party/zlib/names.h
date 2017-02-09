/* Copyright 2017 The Crashpad Authors. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License. */

#ifndef CRASHPAD_THIRD_PARTY_ZLIB_NAMES_H_
#define CRASHPAD_THIRD_PARTY_ZLIB_NAMES_H_

/* Rename all zlib names with a CP_z_ prefix. This is based on the Z_PREFIX
 * option from zconf.h, but with a custom prefix. Where zconf.h would rename
 * both a macro and its underscore-suffixed internal implementation (such as
 * deflateInit2 and deflateInit2_), only the implementation is renamed here. */

#define _dist_code CP_z__dist_code
#define _length_code CP_z__length_code
#define _tr_align CP_z__tr_align
#define _tr_flush_bits CP_z__tr_flush_bits
#define _tr_flush_block CP_z__tr_flush_block
#define _tr_init CP_z__tr_init
#define _tr_stored_block CP_z__tr_stored_block
#define _tr_tally CP_z__tr_tally
#define adler32 CP_z_adler32
#define adler32_combine CP_z_adler32_combine
#define adler32_combine64 CP_z_adler32_combine64
#define adler32_z CP_z_adler32_z
#ifndef Z_SOLO
#define compress CP_z_compress
#define compress2 CP_z_compress2
#define compressBound CP_z_compressBound
#endif
#define crc32 CP_z_crc32
#define crc32_combine CP_z_crc32_combine
#define crc32_combine64 CP_z_crc32_combine64
#define crc32_z CP_z_crc32_z
#define deflate CP_z_deflate
#define deflateBound CP_z_deflateBound
#define deflateCopy CP_z_deflateCopy
#define deflateEnd CP_z_deflateEnd
#define deflateGetDictionaryCP_z_deflateGetDictionary
#define deflateInit2_ CP_z_deflateInit2_
#define deflateInit_ CP_z_deflateInit_
#define deflateParams CP_z_deflateParams
#define deflatePending CP_z_deflatePending
#define deflatePrime CP_z_deflatePrime
#define deflateReset CP_z_deflateReset
#define deflateResetKeep CP_z_deflateResetKeep
#define deflateSetDictionaryCP_z_deflateSetDictionary
#define deflateSetHeader CP_z_deflateSetHeader
#define deflateTune CP_z_deflateTune
#define deflate_copyright CP_z_deflate_copyright
#define get_crc_table CP_z_get_crc_table
#ifndef Z_SOLO
#define gz_error CP_z_gz_error
#define gz_intmax CP_z_gz_intmax
#define gz_strwinerror CP_z_gz_strwinerror
#define gzbuffer CP_z_gzbuffer
#define gzclearerr CP_z_gzclearerr
#define gzclose CP_z_gzclose
#define gzclose_r CP_z_gzclose_r
#define gzclose_w CP_z_gzclose_w
#define gzdirect CP_z_gzdirect
#define gzdopen CP_z_gzdopen
#define gzeof CP_z_gzeof
#define gzerror CP_z_gzerror
#define gzflush CP_z_gzflush
#define gzfread CP_z_gzfread
#define gzfwrite CP_z_gzfwrite
#define gzgetc_ CP_z_gzgetc_
#define gzgets CP_z_gzgets
#define gzoffset CP_z_gzoffset
#define gzoffset64 CP_z_gzoffset64
#define gzopen CP_z_gzopen
#define gzopen64 CP_z_gzopen64
#ifdef _WIN32
#define gzopen_w CP_z_gzopen_w
#endif
#define gzprintf CP_z_gzprintf
#define gzputc CP_z_gzputc
#define gzputs CP_z_gzputs
#define gzread CP_z_gzread
#define gzrewind CP_z_gzrewind
#define gzseek CP_z_gzseek
#define gzseek64 CP_z_gzseek64
#define gzsetparams CP_z_gzsetparams
#define gztell CP_z_gztell
#define gztell64 CP_z_gztell64
#define gzungetc CP_z_gzungetc
#define gzvprintf CP_z_gzvprintf
#define gzwrite CP_z_gzwrite
#endif
#define inflate CP_z_inflate
#define inflateBack CP_z_inflateBack
#define inflateBackEnd CP_z_inflateBackEnd
#define inflateBackInit_ CP_z_inflateBackInit_
#define inflateCodesUsed CP_z_inflateCodesUsed
#define inflateCopy CP_z_inflateCopy
#define inflateEnd CP_z_inflateEnd
#define inflateGetDictionaryCP_z_inflateGetDictionary
#define inflateGetHeader CP_z_inflateGetHeader
#define inflateInit2_ CP_z_inflateInit2_
#define inflateInit_ CP_z_inflateInit_
#define inflateMark CP_z_inflateMark
#define inflatePrime CP_z_inflatePrime
#define inflateReset CP_z_inflateReset
#define inflateReset2 CP_z_inflateReset2
#define inflateResetKeep CP_z_inflateResetKeep
#define inflateSetDictionaryCP_z_inflateSetDictionary
#define inflateSync CP_z_inflateSync
#define inflateSyncPoint CP_z_inflateSyncPoint
#define inflateUndermine CP_z_inflateUndermine
#define inflateValidate CP_z_inflateValidate
#define inflate_copyright CP_z_inflate_copyright
#define inflate_fast CP_z_inflate_fast
#define inflate_table CP_z_inflate_table
#ifndef Z_SOLO
#define uncompress CP_z_uncompress
#define uncompress2 CP_z_uncompress2
#endif
#define zError CP_z_zError
#ifndef Z_SOLO
#define zcalloc CP_z_zcalloc
#define zcfree CP_z_zcfree
#endif
#define zlibCompileFlags CP_z_zlibCompileFlags
#define zlibVersion CP_z_zlibVersion
#define Byte CP_z_Byte
#define Bytef CP_z_Bytef
#define alloc_func CP_z_alloc_func
#define charf CP_z_charf
#define free_func CP_z_free_func
#ifndef Z_SOLO
#define gzFile CP_z_gzFile
#endif
#define gz_header CP_z_gz_header
#define gz_headerp CP_z_gz_headerp
#define in_func CP_z_in_func
#define intf CP_z_intf
#define out_func CP_z_out_func
#define uInt CP_z_uInt
#define uIntf CP_z_uIntf
#define uLong CP_z_uLong
#define uLongf CP_z_uLongf
#define voidp CP_z_voidp
#define voidpc CP_z_voidpc
#define voidpf CP_z_voidpf
#define gz_header_s CP_z_gz_header_s
#define internal_state CP_z_internal_state

#endif  /* CRASHPAD_THIRD_PARTY_ZLIB_NAMES_H_ */
