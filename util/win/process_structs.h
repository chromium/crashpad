// Copyright 2015 The Crashpad Authors. All rights reserved.
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

#ifndef CRASHPAD_UTIL_WIN_PROCESS_STRUCTS_H_
#define CRASHPAD_UTIL_WIN_PROCESS_STRUCTS_H_

#include <windows.h>

namespace crashpad {
namespace process_types {

namespace internal {

struct Traits32 {
  using Pad = DWORD;
  using UnsignedIntegral = DWORD;
  using Pointer = DWORD;
};

struct Traits64 {
  using Pad = DWORD64;
  using UnsignedIntegral = DWORD64;
  using Pointer = DWORD64;
};

}  // namespace internal

//! \{

//! \brief Selected structures from winternl.h, ntddk.h, and `dt ntdll!xxx`,
//!     customized to have both x86 and x64 sizes available.
//!
//! The structure and field names follow the Windows names for clarity. We do,
//! however, use plain integral types rather than pointer types. This is both
//! easier to define, and avoids accidentally treating them as pointers into the
//! current address space.
//!
//! The templates below should be instantiated with either internal::Traits32
//! for structures targeting x86, or internal::Traits64 for x64.

// We set packing to 1 so that we can explicitly control the layout to make it
// match the OS defined structures.
#pragma pack(push, 1)

template <class Traits>
struct PROCESS_BASIC_INFORMATION {
  union {
    DWORD ExitStatus;
    typename Traits::Pad padding_for_x64_0;
  };
  typename Traits::Pointer PebBaseAddress;
  typename Traits::UnsignedIntegral AffinityMask;
  union {
    DWORD BasePriority;
    typename Traits::Pad padding_for_x64_1;
  };
  typename Traits::UnsignedIntegral UniqueProcessId;
  typename Traits::UnsignedIntegral InheritedFromUniqueProcessId;
};

template <class Traits>
struct LIST_ENTRY {
  typename Traits::Pointer Flink;
  typename Traits::Pointer Blink;
};

template <class Traits>
struct UNICODE_STRING {
  union {
    struct {
      USHORT Length;
      USHORT MaximumLength;
    };
    typename Traits::Pad padding_for_x64;
  };
  typename Traits::Pointer Buffer;
};

template <class Traits>
struct PEB_LDR_DATA {
  ULONG Length;
  DWORD Initialized;
  typename Traits::Pointer SsHandle;
  LIST_ENTRY<Traits> InLoadOrderModuleList;
  LIST_ENTRY<Traits> InMemoryOrderModuleList;
  LIST_ENTRY<Traits> InInitializationOrderModuleList;
};

template <class Traits>
struct LDR_DATA_TABLE_ENTRY {
  LIST_ENTRY<Traits> InLoadOrderLinks;
  LIST_ENTRY<Traits> InMemoryOrderLinks;
  LIST_ENTRY<Traits> InInitializationOrderLinks;
  typename Traits::Pointer DllBase;
  typename Traits::Pointer EntryPoint;
  union {
    ULONG SizeOfImage;
    typename Traits::Pad padding_for_x64;
  };
  UNICODE_STRING<Traits> FullDllName;
  UNICODE_STRING<Traits> BaseDllName;
  ULONG Flags;
  USHORT ObsoleteLoadCount;
  USHORT TlsIndex;
  LIST_ENTRY<Traits> HashLinks;
  ULONG TimeDateStamp;
};

template <class Traits>
struct CURDIR {
  UNICODE_STRING<Traits> DosPath;
  typename Traits::Pointer Handle;
};

template <class Traits>
struct STRING {
  union {
    struct {
      DWORD Length;
      DWORD MaximumLength;
    };
    typename Traits::Pad padding_for_x64;
  };
  typename Traits::Pointer Buffer;
};

template <class Traits>
struct RTL_DRIVE_LETTER_CURDIR {
  WORD Flags;
  WORD Length;
  DWORD TimeStamp;
  STRING<Traits> DosPath;
};

template <class Traits>
struct RTL_USER_PROCESS_PARAMETERS {
  DWORD MaximumLength;
  DWORD Length;
  DWORD Flags;
  DWORD DebugFlags;
  typename Traits::Pointer ConsoleHandle;
  union {
    DWORD ConsoleFlags;
    typename Traits::Pad padding_for_x64;
  };
  typename Traits::Pointer StandardInput;
  typename Traits::Pointer StandardOutput;
  typename Traits::Pointer StandardError;
  CURDIR<Traits> CurrentDirectory;
  UNICODE_STRING<Traits> DllPath;
  UNICODE_STRING<Traits> ImagePathName;
  UNICODE_STRING<Traits> CommandLine;
  typename Traits::Pointer Environment;
  DWORD StartingX;
  DWORD StartingY;
  DWORD CountX;
  DWORD CountY;
  DWORD CountCharsX;
  DWORD CountCharsY;
  DWORD FillAttribute;
  DWORD WindowFlags;
  DWORD ShowWindowFlags;
  UNICODE_STRING<Traits> WindowTitle;
  UNICODE_STRING<Traits> DesktopInfo;
  UNICODE_STRING<Traits> ShellInfo;
  UNICODE_STRING<Traits> RuntimeData;
  RTL_DRIVE_LETTER_CURDIR<Traits> CurrentDirectores[32];  // sic.
};

template <class T>
struct GdiHandleBufferCountForBitness;

template <>
struct GdiHandleBufferCountForBitness<internal::Traits32> {
  enum { value = 34 };
};
template <>
struct GdiHandleBufferCountForBitness<internal::Traits64> {
  enum { value = 60 };
};

template <class Traits>
struct PEB {
  union {
    struct {
      BYTE InheritedAddressSpace;
      BYTE ReadImageFileExecOptions;
      BYTE BeingDebugged;
      BYTE BitField;
    };
    typename Traits::Pad padding_for_x64_0;
  };
  typename Traits::Pointer Mutant;
  typename Traits::Pointer ImageBaseAddress;
  typename Traits::Pointer Ldr;
  typename Traits::Pointer ProcessParameters;
  typename Traits::Pointer SubSystemData;
  typename Traits::Pointer ProcessHeap;
  typename Traits::Pointer FastPebLock;
  typename Traits::Pointer AtlThunkSListPtr;
  typename Traits::Pointer IFEOKey;
  union {
    DWORD CrossProcessFlags;
    typename Traits::Pad padding_for_x64_1;
  };
  typename Traits::Pointer KernelCallbackTable;
  DWORD SystemReserved;
  DWORD AtlThunkSListPtr32;
  typename Traits::Pointer ApiSetMap;
  union {
    DWORD TlsExpansionCounter;
    typename Traits::Pad padding_for_x64_2;
  };
  typename Traits::Pointer TlsBitmap;
  DWORD TlsBitmapBits[2];
  typename Traits::Pointer ReadOnlySharedMemoryBase;
  typename Traits::Pointer SparePvoid0;
  typename Traits::Pointer ReadOnlyStaticServerData;
  typename Traits::Pointer AnsiCodePageData;
  typename Traits::Pointer OemCodePageData;
  typename Traits::Pointer UnicodeCaseTableData;
  DWORD NumberOfProcessors;
  DWORD NtGlobalFlag;
  LARGE_INTEGER CriticalSectionTimeout;
  typename Traits::UnsignedIntegral HeapSegmentReserve;
  typename Traits::UnsignedIntegral HeapSegmentCommit;
  typename Traits::UnsignedIntegral HeapDeCommitTotalFreeThreshold;
  typename Traits::UnsignedIntegral HeapDeCommitFreeBlockThreshold;
  DWORD NumberOfHeaps;
  DWORD MaximumNumberOfHeaps;
  typename Traits::Pointer ProcessHeaps;
  typename Traits::Pointer GdiSharedHandleTable;
  typename Traits::Pointer ProcessStarterHelper;
  DWORD GdiDCAttributeList;
  typename Traits::Pointer LoaderLock;
  DWORD OSMajorVersion;
  DWORD OSMinorVersion;
  WORD OSBuildNumber;
  WORD OSCSDVersion;
  DWORD OSPlatformId;
  DWORD ImageSubsystem;
  DWORD ImageSubsystemMajorVersion;
  union {
    DWORD ImageSubsystemMinorVersion;
    typename Traits::Pad padding_for_x64_3;
  };
  typename Traits::UnsignedIntegral ActiveProcessAffinityMask;
  DWORD GdiHandleBuffer[GdiHandleBufferCountForBitness<Traits>::value];
  typename Traits::Pointer PostProcessInitRoutine;
  typename Traits::Pointer TlsExpansionBitmap;
  DWORD TlsExpansionBitmapBits[32];
  union {
    DWORD SessionId;
    typename Traits::Pad padding_for_x64_4;
  };
  ULARGE_INTEGER AppCompatFlags;
  ULARGE_INTEGER AppCompatFlagsUser;
  typename Traits::Pointer pShimData;
  typename Traits::Pointer AppCompatInfo;
  UNICODE_STRING<Traits> CSDVersion;
  typename Traits::Pointer ActivationContextData;
  typename Traits::Pointer ProcessAssemblyStorageMap;
  typename Traits::Pointer SystemDefaultActivationContextData;
  typename Traits::Pointer SystemAssemblyStorageMap;
  typename Traits::UnsignedIntegral MinimumStackCommit;
  typename Traits::Pointer FlsCallback;
  LIST_ENTRY<Traits> FlsListHead;
  typename Traits::Pointer FlsBitmap;
  DWORD FlsBitmapBits[4];
  DWORD FlsHighIndex;
};

template <class Traits>
struct NT_TIB {
  typename Traits::Pointer ExceptionList;
  typename Traits::Pointer StackBase;
  typename Traits::Pointer StackLimit;
  typename Traits::Pointer SubSystemTib;
  union {
    typename Traits::Pointer FiberData;
    BYTE Version[4];
  };
  typename Traits::Pointer ArbitraryUserPointer;
  typename Traits::Pointer Self;
};

// See https://msdn.microsoft.com/en-us/library/gg750647.aspx.
template <class Traits>
struct CLIENT_ID {
  typename Traits::Pointer UniqueProcess;
  typename Traits::Pointer UniqueThread;
};

// This is a partial definition of the TEB, as we do not currently use many
// fields of it. See http://www.nirsoft.net/kernel_struct/vista/TEB.html.
template <class Traits>
struct TEB {
  NT_TIB<Traits> NtTib;
  typename Traits::Pointer EnvironmentPointer;
  CLIENT_ID<Traits> ClientId;
};

// See https://msdn.microsoft.com/en-us/library/gg750724.aspx for the base
// structure, and 
// http://processhacker.sourceforge.net/doc/struct___s_y_s_t_e_m___e_x_t_e_n_d_e_d___t_h_r_e_a_d___i_n_f_o_r_m_a_t_i_o_n.html
// for the extension part.
template <class Traits>
struct SYSTEM_EXTENDED_THREAD_INFORMATION {
  LARGE_INTEGER KernelTime;
  LARGE_INTEGER UserTime;
  LARGE_INTEGER CreateTime;
  union {
    ULONG WaitTime;
    typename Traits::Pad padding_for_x64_0;
  };
  typename Traits::Pointer StartAddress;
  CLIENT_ID<Traits> ClientId;
  LONG Priority;
  LONG BasePriority;
  ULONG ContextSwitches;
  ULONG ThreadState;
  union {
    ULONG WaitReason;
    typename Traits::Pad padding_for_x64_1;
  };
  typename Traits::Pointer StackBase;  // These don't appear to be correct.
  typename Traits::Pointer StackLimit;
  typename Traits::Pointer Win32StartAddress;
  typename Traits::Pointer TebBase;
  typename Traits::Pointer Reserved;
  typename Traits::Pointer Reserved2;
  typename Traits::Pointer Reserved3;
};

// See http://undocumented.ntinternals.net/source/usermode/undocumented%20functions/system%20information/structures/system_process_information.html
template <class Traits>
struct SYSTEM_PROCESS_INFORMATION {
  ULONG NextEntryOffset;
  ULONG NumberOfThreads;
  LARGE_INTEGER Reserved[3];
  LARGE_INTEGER CreateTime;
  LARGE_INTEGER UserTime;
  LARGE_INTEGER KernelTime;
  UNICODE_STRING<Traits> ImageName;
  union {
    LONG BasePriority;
    typename Traits::Pad padding_for_x64_0;
  };
  union {
    DWORD UniqueProcessId;
    typename Traits::Pad padding_for_x64_1;
  };
  union {
    DWORD InheritedFromUniqueProcessId;
    typename Traits::Pad padding_for_x64_2;
  };
  ULONG HandleCount;
  ULONG Reserved2[3];
  SIZE_T PeakVirtualSize;
  SIZE_T VirtualSize;
  union {
    ULONG PageFaultCount;
    typename Traits::Pad padding_for_x64_3;
  };
  SIZE_T PeakWorkingSetSize;
  SIZE_T WorkingSetSize;
  SIZE_T QuotaPeakPagedPoolUsage;
  SIZE_T QuotaPagedPoolUsage;
  SIZE_T QuotaPeakNonPagedPoolUsage;
  SIZE_T QuotaNonPagedPoolUsage;
  SIZE_T PagefileUsage;
  SIZE_T PeakPagefileUsage;
  SIZE_T PrivatePageCount;
  LARGE_INTEGER ReadOperationCount;
  LARGE_INTEGER WriteOperationCount;
  LARGE_INTEGER OtherOperationCount;
  LARGE_INTEGER ReadTransferCount;
  LARGE_INTEGER WriteTransferCount;
  LARGE_INTEGER OtherTransferCount;
  SYSTEM_EXTENDED_THREAD_INFORMATION<Traits> Threads[1];
};

#pragma pack(pop)

//! \}

}  // namespace process_types
}  // namespace crashpad

#endif  // CRASHPAD_UTIL_WIN_PROCESS_STRUCTS_H_
