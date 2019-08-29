// Copyright 2019 The Crashpad Authors. All rights reserved.
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

#pragma once

#include <lib/zx/vmo.h>

#include "util/file/file_writer.h"

namespace crashpad {

class VMOFileWriter : public FileWriterInterface {
 public:
  ~VMOFileWriter() override;

  // FileSeeker Implementation.

  FileOffset Seek(FileOffset offset, int whence) override;

  // FileWriter Interface Implementation.

  bool Write(const void* data, size_t size) override;

  bool WriteIoVec(std::vector<WritableIoVec>* iovecs) override;

  zx_status_t GenerateVMO(zx::vmo* out) const;

 private:
  int offset_ = 0;
  std::vector<uint8_t> data_;
};

}  // namespace crashpad
