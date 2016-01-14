// Copyright 2016 The Crashpad Authors. All rights reserved.
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

#ifndef CRASHPAD_SNAPSHOT_WIN_CAPTURE_REFERENCED_MEMORY_H_
#define CRASHPAD_SNAPSHOT_WIN_CAPTURE_REFERENCED_MEMORY_H_

#include "snapshot/cpu_context.h"
#include "snapshot/win/process_reader_win.h"
#include "util/stdlib/pointer_container.h"

namespace crashpad {
namespace internal {

class MemorySnapshotWin;

//! \brief For all registers that appear to be pointer-like in \a context,
//!     captures a small amount of memory near their pointed to location.
//!
//! \param[in] context The context to inspect.
//! \param[in] process_reader A ProcessReaderWin to read from the target
//!     process.
//! \param[in] thread The thread to which the context belongs.
//! \param[out] into A vector of pointers to append new ranges to.
void CaptureMemoryPointedToByContext(const CPUContext& context,
                                     ProcessReaderWin* process_reader,
                                     const ProcessReaderWin::Thread& thread,
                                     PointerVector<MemorySnapshotWin>* into);

//! \brief For all pointer-like values in a memory range of the target process,
//!     captures a small amount of memory near the pointed to location.
//!
//! \param[in] memory An existing MemorySnapshotWin of the range to search. The
//!     base address and size must be pointer-aligned and an integral number of
//!     pointers long.
//! \param[in] process_reader A ProcessReaderWin to read from the target
//!     process.
//! \param[out] into A vector of pointers to append new ranges to.
void CaptureMemoryPointedToByMemoryRange(
    const MemorySnapshotWin& memory,
    ProcessReaderWin* process_reader,
    PointerVector<MemorySnapshotWin>* into);

}  // namespace internal
}  // namespace crashpad

#endif  // CRASHPAD_SNAPSHOT_WIN_CAPTURE_REFERENCED_MEMORY_H_
