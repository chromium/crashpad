// Copyright 2014 The Crashpad Authors. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef CRASHPAD_MINIDUMP_MINIDUMP_EXTENSIONS_H_
#define CRASHPAD_MINIDUMP_MINIDUMP_EXTENSIONS_H_

#include <dbghelp.h>
#include <stdint.h>
#include <winnt.h>

#include "util/misc/uuid.h"

namespace crashpad {

//! \brief Minidump stream type values for MINIDUMP_DIRECTORY::StreamType. Each
//!     stream structure has a corresponding stream type value to identify it.
//!
//! \sa MINIDUMP_STREAM_TYPE
enum MinidumpStreamType : uint32_t {
  //! \brief The stream type for MINIDUMP_THREAD_LIST.
  //!
  //! \sa ThreadListStream
  kMinidumpStreamTypeThreadList = ThreadListStream,

  //! \brief The stream type for MINIDUMP_MODULE_LIST.
  //!
  //! \sa ModuleListStream
  kMinidumpStreamTypeModuleList = ModuleListStream,

  //! \brief The stream type for MINIDUMP_MEMORY_LIST.
  //!
  //! \sa MemoryListStream
  kMinidumpStreamTypeMemoryList = MemoryListStream,

  //! \brief The stream type for MINIDUMP_EXCEPTION_STREAM.
  //!
  //! \sa ExceptionStream
  kMinidumpStreamTypeException = ExceptionStream,

  //! \brief The stream type for MINIDUMP_SYSTEM_INFO.
  //!
  //! \sa SystemInfoStream
  kMinidumpStreamTypeSystemInfo = SystemInfoStream,

  //! \brief The stream type for MINIDUMP_MISC_INFO, MINIDUMP_MISC_INFO_2,
  //!     MINIDUMP_MISC_INFO_3, and MINIDUMP_MISC_INFO_4.
  //!
  //! \sa MiscInfoStream
  kMinidumpStreamTypeMiscInfo = MiscInfoStream,

  // 0x4350 = "CP"

  //! \brief The stream type for MinidumpCrashpadInfo.
  kMinidumpStreamTypeCrashpadInfo = 0x43500001,
};

//! \brief A variable-length UTF-8-encoded string carried within a minidump
//!     file.
//!
//! \sa MINIDUMP_STRING
struct __attribute__((packed, aligned(4))) MinidumpUTF8String {
  // The field names do not conform to typical style, they match the names used
  // in MINIDUMP_STRING. This makes it easier to operate on MINIDUMP_STRING (for
  // UTF-16 strings) and MinidumpUTF8String using templates.

  //! \brief The length of the #Buffer field in bytes, not including the `NUL`
  //!     terminator.
  //!
  //! \note This field is interpreted as a byte count, not a count of Unicode
  //!     code points.
  uint32_t Length;

  //! \brief The string, encoded in UTF-8, and terminated with a `NUL` byte.
  uint8_t Buffer[0];
};

//! \brief CPU type values for MINIDUMP_SYSTEM_INFO::ProcessorArchitecture.
//!
//! \sa \ref PROCESSOR_ARCHITECTURE_x "PROCESSOR_ARCHITECTURE_*"
enum MinidumpCPUArchitecture : uint16_t {
  //! \brief 32-bit x86.
  //!
  //! These systems identify their CPUs generically as “x86” or “ia32”, or with
  //! more specific names such as “i386”, “i486”, “i586”, and “i686”.
  kMinidumpCPUArchitectureX86 = PROCESSOR_ARCHITECTURE_INTEL,

  kMinidumpCPUArchitectureMIPS = PROCESSOR_ARCHITECTURE_MIPS,
  kMinidumpCPUArchitectureAlpha = PROCESSOR_ARCHITECTURE_ALPHA,

  //! \brief 32-bit PowerPC.
  //!
  //! These systems identify their CPUs generically as “ppc”, or with more
  //! specific names such as “ppc6xx”, “ppc7xx”, and “ppc74xx”.
  kMinidumpCPUArchitecturePPC = PROCESSOR_ARCHITECTURE_PPC,

  kMinidumpCPUArchitectureSHx = PROCESSOR_ARCHITECTURE_SHX,

  //! \brief 32-bit ARM.
  //!
  //! These systems identify their CPUs generically as “arm”, or with more
  //! specific names such as “armv6” and “armv7”.
  kMinidumpCPUArchitectureARM = PROCESSOR_ARCHITECTURE_ARM,

  kMinidumpCPUArchitectureIA64 = PROCESSOR_ARCHITECTURE_IA64,
  kMinidumpCPUArchitectureAlpha64 = PROCESSOR_ARCHITECTURE_ALPHA64,
  kMinidumpCPUArchitectureMSIL = PROCESSOR_ARCHITECTURE_MSIL,

  //! \brief 64-bit x86.
  //!
  //! These systems identify their CPUs as “x86_64”, “amd64”, or “x64”.
  kMinidumpCPUArchitectureAMD64 = PROCESSOR_ARCHITECTURE_AMD64,

  //! \brief A 32-bit x86 process running on IA-64 (Itanium).
  //!
  //! \note This value is not used in minidump files for 32-bit x86 processes
  //!     running on a 64-bit-capable x86 CPU and operating system. In that
  //!     configuration, #kMinidumpCPUArchitectureX86 is used instead.
  kMinidumpCPUArchitectureX86Win64 = PROCESSOR_ARCHITECTURE_IA32_ON_WIN64,

  kMinidumpCPUArchitectureNeutral = PROCESSOR_ARCHITECTURE_NEUTRAL,
  kMinidumpCPUArchitectureSPARC = 0x8001,

  //! \brief 64-bit PowerPC.
  //!
  //! These systems identify their CPUs generically as “ppc64”, or with more
  //! specific names such as “ppc970”.
  kMinidumpCPUArchitecturePPC64 = 0x8002,

  //! \brief 64-bit ARM.
  //!
  //! These systems identify their CPUs generically as “arm64” or “aarch64”, or
  //! with more specific names such as “armv8”.
  kMinidumpCPUArchitectureARM64 = 0x8003,

  //! \brief Unknown CPU architecture.
  kMinidumpCPUArchitectureUnknown = PROCESSOR_ARCHITECTURE_UNKNOWN,
};

//! \brief Operating system type values for MINIDUMP_SYSTEM_INFO::ProductType.
//!
//! \sa \ref VER_NT_x "VER_NT_*"
enum MinidumpOSType : uint8_t {
  //! \brief A “desktop” or “workstation” system.
  kMinidumpOSTypeWorkstation = VER_NT_WORKSTATION,

  //! \brief A “domain controller” system. Windows-specific.
  kMinidumpOSTypeDomainController = VER_NT_DOMAIN_CONTROLLER,

  //! \brief A “server” system.
  kMinidumpOSTypeServer = VER_NT_SERVER,
};

//! \brief Operating system family values for MINIDUMP_SYSTEM_INFO::PlatformId.
//!
//! \sa \ref VER_PLATFORM_x "VER_PLATFORM_*"
enum MinidumpOS : uint32_t {
  //! \brief Windows 3.1.
  kMinidumpOSWin32s = VER_PLATFORM_WIN32s,

  //! \brief Windows 95, Windows 98, and Windows Me.
  kMinidumpOSWin32Windows = VER_PLATFORM_WIN32_WINDOWS,

  //! \brief Windows NT, Windows 2000, and later.
  kMinidumpOSWin32NT = VER_PLATFORM_WIN32_NT,

  kMinidumpOSUnix = 0x8000,

  //! \brief Mac OS X, Darwin for traditional systems.
  kMinidumpOSMacOSX = 0x8101,

  //! \brief iOS, Darwin for mobile devices.
  kMinidumpOSiOS = 0x8102,

  //! \brief Linux, not including Android.
  kMinidumpOSLinux = 0x8201,

  kMinidumpOSSolaris = 0x8202,

  //! \brief Android.
  kMinidumpOSAndroid = 0x8203,

  kMinidumpOSPS3 = 0x8204,

  //! \brief Native Client (NaCl).
  kMinidumpOSNaCl = 0x8205,

  //! \brief Unknown operating system.
  kMinidumpOSUnknown = 0xffffffff,
};

//! \brief A CodeView record linking to a `.pdb` 2.0 file.
//!
//! This format provides an indirect link to debugging data by referencing an
//! external `.pdb` file by its name, timestamp, and age. This structure may be
//! pointed to by MINIDUMP_MODULE::CvRecord. It has been superseded by
//! MinidumpModuleCodeViewRecordPDB70.
//!
//! For more information about this structure and format, see <a
//! href="http://www.debuginfo.com/articles/debuginfomatch.html#pdbfiles">Matching
//! Debug Information</a>, PDB Files, and <a
//! href="http://undocumented.rawol.com/sbs-w2k-1-windows-2000-debugging-support.pdf#page=63">Undocumented
//! Windows 2000 Secrets</a>, Windows 2000 Debugging Support/Microsoft Symbol
//! File Internals/CodeView Subsections.
//!
//! \sa IMAGE_DEBUG_MISC
struct MinidumpModuleCodeViewRecordPDB20 {
  //! \brief The magic number identifying this structure version, stored in
  //!     #signature.
  //!
  //! In a hex dump, this will appear as “NB10” when produced by a little-endian
  //! machine.
  static const uint32_t kSignature = '01BN';

  //! \brief The magic number identifying this structure version, the value of
  //!     #kSignature.
  uint32_t signature;

  //! \brief The offset to CodeView data.
  //!
  //! In this structure, this field always has the value `0` because no CodeView
  //! data is present, there is only a link to CodeView data stored in an
  //! external file.
  uint32_t offset;

  //! \brief The time that the `.pdb` file was created, in `time_t` format, the
  //!     number of seconds since the POSIX epoch.
  uint32_t timestamp;

  //! \brief The revision of the `.pdb` file.
  //!
  //! A `.pdb` file’s age indicates incremental changes to it. When a `.pdb`
  //! file is created, it has age `1`, and subsequent updates increase this
  //! value.
  uint32_t age;

  //! \brief The path or file name of the `.pdb` file associated with the
  //!     module.
  //!
  //! This is a NUL-terminated string. On Windows, it will be encoded in the
  //! code page of the system that linked the module. On other operating
  //! systems, UTF-8 may be used.
  uint8_t pdb_name[1];
};

//! \brief A CodeView record linking to a `.pdb` 7.0 file.
//!
//! This format provides an indirect link to debugging data by referencing an
//! external `.pdb` file by its name, %UUID, and age. This structure may be
//! pointed to by MINIDUMP_MODULE::CvRecord.
//!
//! For more information about this structure and format, see <a
//! href="http://www.debuginfo.com/articles/debuginfomatch.html#pdbfiles">Matching
//! Debug Information</a>, PDB Files.
//!
//! \sa MinidumpModuleCodeViewRecordPDB20
//! \sa IMAGE_DEBUG_MISC
struct MinidumpModuleCodeViewRecordPDB70 {
  //! \brief The magic number identifying this structure version, stored in
  //!     #signature.
  //!
  //! In a hex dump, this will appear as “RSDS” when produced by a little-endian
  //! machine.
  static const uint32_t kSignature = 'SDSR';

  //! \brief The magic number identifying this structure version, the value of
  //!     #kSignature.
  uint32_t signature;

  //! \brief The `.pdb` file’s unique identifier.
  UUID uuid;

  //! \brief The revision of the `.pdb` file.
  //!
  //! A `.pdb` file’s age indicates incremental changes to it. When a `.pdb`
  //! file is created, it has age `1`, and subsequent updates increase this
  //! value.
  uint32_t age;

  //! \brief The path or file name of the `.pdb` file associated with the
  //!     module.
  //!
  //! This is a NUL-terminated string. On Windows, it will be encoded in the
  //! code page of the system that linked the module. On other operating
  //! systems, UTF-8 may be used.
  uint8_t pdb_name[1];
};

//! \brief A list of ::RVA pointers.
struct __attribute__((packed, aligned(4))) MinidumpRVAList {
  //! \brief The number of children present in the #children array.
  uint32_t count;

  //! \brief Pointers to other structures in the minidump file.
  RVA children[0];
};

//! \brief A list of MINIDUMP_LOCATION_DESCRIPTOR objects.
struct __attribute__((packed, aligned(4))) MinidumpLocationDescriptorList {
  //! \brief The number of children present in the #children array.
  uint32_t count;

  //! \brief Pointers to other structures in the minidump file.
  MINIDUMP_LOCATION_DESCRIPTOR children[0];
};

//! \brief A key-value pair.
struct __attribute__((packed, aligned(4))) MinidumpSimpleStringDictionaryEntry {
  //! \brief ::RVA of a MinidumpUTF8String containing the key of a key-value
  //!     pair.
  RVA key;

  //! \brief ::RVA of a MinidumpUTF8String containing the value of a key-value
  //!     pair.
  RVA value;
};

//! \brief A list of key-value pairs.
struct __attribute__((packed, aligned(4))) MinidumpSimpleStringDictionary {
  //! \brief The number of key-value pairs present.
  uint32_t count;

  //! \brief A list of MinidumpSimpleStringDictionaryEntry entries.
  MinidumpSimpleStringDictionaryEntry entries[0];
};

//! \brief Additional Crashpad-specific information about a module carried
//!     within a minidump file.
//!
//! This structure augments the information provided by MINIDUMP_MODULE. The
//! minidump file must contain a module list stream
//! (::kMinidumpStreamTypeModuleList) in order for this structure to appear.
//!
//! This structure is versioned. When changing this structure, leave the
//! existing structure intact so that earlier parsers will be able to understand
//! the fields they are aware of, and make additions at the end of the
//! structure. Revise #kVersion and document each field’s validity based on
//! #version, so that newer parsers will be able to determine whether the added
//! fields are valid or not.
//!
//! \sa #MinidumpModuleCrashpadInfoList
struct __attribute__((packed, aligned(4))) MinidumpModuleCrashpadInfo {
  //! \brief The structure’s currently-defined version number.
  //!
  //! \sa version
  static const uint32_t kVersion = 1;

  //! \brief The structure’s version number.
  //!
  //! Readers can use this field to determine which other fields in the
  //! structure are valid. Upon encountering a value greater than #kVersion, a
  //! reader should assume that the structure’s layout is compatible with the
  //! structure defined as having value #kVersion.
  //!
  //! Writers may produce values less than #kVersion in this field if there is
  //! no need for any fields present in later versions.
  uint32_t version;

  //! \brief A link to a MINIDUMP_MODULE structure in the module list stream.
  //!
  //! This field is an index into MINIDUMP_MODULE_LIST::Modules. This field’s
  //! value must be in the range of MINIDUMP_MODULE_LIST::NumberOfEntries.
  //!
  //! This field is present when #version is at least `1`.
  uint32_t minidump_module_list_index;

  //! \brief A MinidumpRVAList pointing to MinidumpUTF8String objects. The
  //!     module controls the data that appears here.
  //!
  //! These strings correspond to ModuleSnapshot::AnnotationsVector() and do not
  //! duplicate anything in #simple_annotations.
  //!
  //! This field is present when #version is at least `1`.
  MINIDUMP_LOCATION_DESCRIPTOR list_annotations;

  //! \brief A MinidumpSimpleStringDictionary pointing to strings interpreted as
  //!     key-value pairs. The module controls the data that appears here.
  //!
  //! These key-value pairs correspond to
  //! ModuleSnapshot::AnnotationsSimpleMap() and do not duplicate anything in
  //! #list_annotations.
  //!
  //! This field is present when #version is at least `1`.
  MINIDUMP_LOCATION_DESCRIPTOR simple_annotations;
};

//! \brief Additional Crashpad-specific information about modules carried within
//!     a minidump file.
//!
//! This structure augments the information provided by
//! MINIDUMP_MODULE_LIST. The minidump file must contain a module list stream
//! (::kMinidumpStreamTypeModuleList) in order for this structure to appear.
//!
//! MinidumpModuleCrashpadInfoList::count may be less than the value of
//! MINIDUMP_MODULE_LIST::NumberOfModules because not every MINIDUMP_MODULE
//! structure carried within the minidump file will necessarily have
//! Crashpad-specific information provided by a MinidumpModuleCrashpadInfo
//! structure.
//!
//! MinidumpModuleCrashpadInfoList::children references
//! MinidumpModuleCrashpadInfo children indirectly through
//! MINIDUMP_LOCATION_DESCRIPTOR pointers to allow for future growth of the
//! MinidumpModuleCrashpadInfo structure.
using MinidumpModuleCrashpadInfoList = MinidumpLocationDescriptorList;

//! \brief Additional Crashpad-specific information carried within a minidump
//!     file.
//!
//! This structure is versioned. When changing this structure, leave the
//! existing structure intact so that earlier parsers will be able to understand
//! the fields they are aware of, and make additions at the end of the
//! structure. Revise #kVersion and document each field’s validity based on
//! #version, so that newer parsers will be able to determine whether the added
//! fields are valid or not.
struct __attribute__((packed, aligned(4))) MinidumpCrashpadInfo {
  //! \brief The structure’s currently-defined version number.
  //!
  //! \sa version
  static const uint32_t kVersion = 1;

  //! \brief The structure’s version number.
  //!
  //! Readers can use this field to determine which other fields in the
  //! structure are valid. Upon encountering a value greater than #kVersion, a
  //! reader should assume that the structure’s layout is compatible with the
  //! structure defined as having value #kVersion.
  //!
  //! Writers may produce values less than #kVersion in this field if there is
  //! no need for any fields present in later versions.
  uint32_t version;

  //! \brief A pointer to a #MinidumpModuleCrashpadInfoList structure.
  //!
  //! This field is present when #version is at least `1`.
  MINIDUMP_LOCATION_DESCRIPTOR module_list;
};

}  // namespace crashpad

#endif  // CRASHPAD_MINIDUMP_MINIDUMP_EXTENSIONS_H_
