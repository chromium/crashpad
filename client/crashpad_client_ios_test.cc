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

#include "client/crashpad_client.h"

#include <fcntl.h>
#include <sys/mman.h>

#include "gtest/gtest.h"
#include "pack_ios_state.h"
#include "test/scoped_temp_dir.h"

namespace crashpad {
namespace test {
namespace {

// TODO(justincohen): This is a placeholder.
TEST(CrashpadClient, DumpWithoutCrash) {
  crashpad::CrashpadClient client;
  client.StartCrashpadInProcessHandler();
  //  client.DumpWithoutCrash(nullptr);

  // should be wrapped somewhere else.
  ScopedTempDir temp_dir;
  base::FilePath file(temp_dir.path().Append(FILE_PATH_LITERAL("file")));
  int fd = -1;

  int mask = O_WRONLY | O_CREAT | O_TRUNC;
  fd = open_dprotected_np(file.value().c_str(), mask, 4, 0, 0644);

  // Also wrapped somewhere else.
  uint8_t version = 1;

  // Belongs somewhere in the crash handler in a in-proc safe place
  Property(fd, "version", &version, sizeof(version));
  ArrayStart(fd, "modules");
  ArrayObjectStart(fd);
  Property(fd, "modname1", "fooooo", strlen("fooooo"));
  ObjectEnd(fd);
  ArrayObjectStart(fd);
  Property(fd, "modname2", "barrrasdfasd", strlen("barrrasdfasd"));
  ObjectEnd(fd);
  ArrayEnd(fd);
  ObjectStart(fd, "subobject");
  Property(fd, "subojmodname1", "fooooo", strlen("fooooo"));
  ObjectEnd(fd);
  close(fd);

  // Back in the in-proc-after-crash-restart area.
  fd = open(file.value().c_str(), O_RDONLY);
  struct stat filestat;
  fstat(fd, &filestat);
  void* region = mmap(NULL, filestat.st_size, PROT_READ, MAP_SHARED, fd, 0);
  PackedMap in_process_dump =
      Parse(static_cast<uint8_t*>(region), filestat.st_size);

  // Need to return size to iterate arrays and contains to check maps.
  const PackedData foo = in_process_dump["modules"][0]["modname1"].get_data();
  std::string str((char*)foo.data, foo.length);
  ASSERT_EQ(str, "fooooo");

  munmap(region, filestat.st_size);
  close(fd);
}

}  // namespace
}  // namespace test
}  // namespace crashpad
