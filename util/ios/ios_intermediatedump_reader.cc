// Copyright 2020 The Crashpad Authors. All rights reserved.
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

#include "util/ios/ios_intermediatedump_reader.h"

#include <fcntl.h>
#include <sys/stat.h>

namespace crashpad {
namespace internal {

bool IOSIntermediatedumpReader::Initialize(const base::FilePath& dump_path) {
  fd_ = base::ScopedFD(open(dump_path.value().c_str(), O_RDONLY));
  if (!fd_.is_valid())
    return false;

  struct stat filestat;
  fstat(fd_.get(), &filestat);
  if (filestat.st_size == 0) {
    return false;
  }
  mapping_.ResetMmap(
      NULL, filestat.st_size, PROT_READ, MAP_SHARED, fd_.get(), 0);

  minidump_ = IOSIntermediatedumpMap::Parse(mapping_.addr_as<uint8_t*>(),
                                            mapping_.len());
  // Not for landing...
#ifndef NDEBUG
  minidump_.DebugDump();
#endif
  return true;
}

}  // namespace internal
}  // namespace crashpad
