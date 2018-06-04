// Copyright 2018 The Crashpad Authors. All rights reserved.
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

#include <inttypes.h>

#include "minidump/minidump_file_writer.h"
#include "snapshot/minidump/process_snapshot_minidump.h"
#include "test/scoped_temp_dir.h"
#include "util/file/file_reader.h"
#include "util/file/file_writer.h"

using namespace crashpad;
using namespace crashpad::test;

extern "C" int LLVMFuzzerInitialize(int* argc, char*** argv) {
  // Swallow all logs to avoid spam.
  logging::SetLogMessageHandler(
      [](logging::LogSeverity, const char*, int, size_t, const std::string&) {
        return true;
      });
  return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size) {
  {
    ScopedTempDir tempdir;

    base::FilePath input = tempdir.path().Append("input");
    {
      FileWriter input_data;
      CHECK(input_data.Open(
          input, FileWriteMode::kCreateOrFail, FilePermissions::kOwnerOnly));
      input_data.Write(Data, Size);
    }

    FileReader reader;
    CHECK(reader.Open(input));

    ProcessSnapshotMinidump snapshot;
    if (snapshot.Initialize(&reader)) {
      MinidumpFileWriter minidump;
      minidump.InitializeFromSnapshot(&snapshot);

      {
        FileWriter writer;
        CHECK(writer.Open(tempdir.path().Append("test.dmp"),
                          FileWriteMode::kCreateOrFail,
                          FilePermissions::kOwnerOnly));
        minidump.WriteEverything(&writer);
      }
    }
  }

  return 0;
}
