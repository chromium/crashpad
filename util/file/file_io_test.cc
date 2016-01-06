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

#include "util/file/file_io.h"

#include "base/atomicops.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "gtest/gtest.h"
#include "test/errors.h"
#include "test/file.h"
#include "test/scoped_temp_dir.h"
#include "util/misc/implicit_cast.h"
#include "util/thread/thread.h"

namespace crashpad {
namespace test {
namespace {

void TestOpenFileForWrite(FileHandle (*opener)(const base::FilePath&,
                                               FileWriteMode,
                                               FilePermissions)) {
  ScopedTempDir temp_dir;
  base::FilePath file_path_1 =
      temp_dir.path().Append(FILE_PATH_LITERAL("file_1"));
  ASSERT_FALSE(FileExists(file_path_1));

  ScopedFileHandle file_handle(opener(file_path_1,
                                      FileWriteMode::kReuseOrFail,
                                      FilePermissions::kWorldReadable));
  EXPECT_EQ(kInvalidFileHandle, file_handle);
  EXPECT_FALSE(FileExists(file_path_1));

  file_handle.reset(opener(file_path_1,
                           FileWriteMode::kCreateOrFail,
                           FilePermissions::kWorldReadable));
  EXPECT_NE(kInvalidFileHandle, file_handle);
  EXPECT_TRUE(FileExists(file_path_1));
  EXPECT_EQ(0, FileSize(file_path_1));

  file_handle.reset(opener(file_path_1,
                           FileWriteMode::kReuseOrCreate,
                           FilePermissions::kWorldReadable));
  EXPECT_NE(kInvalidFileHandle, file_handle);
  EXPECT_TRUE(FileExists(file_path_1));
  EXPECT_EQ(0, FileSize(file_path_1));

  const char data = '%';
  EXPECT_TRUE(LoggingWriteFile(file_handle.get(), &data, sizeof(data)));

  // Close file_handle to ensure that the write is flushed to disk.
  file_handle.reset();
  EXPECT_EQ(implicit_cast<FileOffset>(sizeof(data)), FileSize(file_path_1));

  file_handle.reset(opener(file_path_1,
                           FileWriteMode::kReuseOrCreate,
                           FilePermissions::kWorldReadable));
  EXPECT_NE(kInvalidFileHandle, file_handle);
  EXPECT_TRUE(FileExists(file_path_1));
  EXPECT_EQ(implicit_cast<FileOffset>(sizeof(data)), FileSize(file_path_1));

  file_handle.reset(opener(file_path_1,
                           FileWriteMode::kCreateOrFail,
                           FilePermissions::kWorldReadable));
  EXPECT_EQ(kInvalidFileHandle, file_handle);
  EXPECT_TRUE(FileExists(file_path_1));
  EXPECT_EQ(implicit_cast<FileOffset>(sizeof(data)), FileSize(file_path_1));

  file_handle.reset(opener(file_path_1,
                           FileWriteMode::kReuseOrFail,
                           FilePermissions::kWorldReadable));
  EXPECT_NE(kInvalidFileHandle, file_handle);
  EXPECT_TRUE(FileExists(file_path_1));
  EXPECT_EQ(implicit_cast<FileOffset>(sizeof(data)), FileSize(file_path_1));

  file_handle.reset(opener(file_path_1,
                           FileWriteMode::kTruncateOrCreate,
                           FilePermissions::kWorldReadable));
  EXPECT_NE(kInvalidFileHandle, file_handle);
  EXPECT_TRUE(FileExists(file_path_1));
  EXPECT_EQ(0, FileSize(file_path_1));

  base::FilePath file_path_2 =
      temp_dir.path().Append(FILE_PATH_LITERAL("file_2"));
  ASSERT_FALSE(FileExists(file_path_2));

  file_handle.reset(opener(file_path_2,
                           FileWriteMode::kTruncateOrCreate,
                           FilePermissions::kWorldReadable));
  EXPECT_NE(kInvalidFileHandle, file_handle);
  EXPECT_TRUE(FileExists(file_path_2));
  EXPECT_EQ(0, FileSize(file_path_2));

  base::FilePath file_path_3 =
      temp_dir.path().Append(FILE_PATH_LITERAL("file_3"));
  ASSERT_FALSE(FileExists(file_path_3));

  file_handle.reset(opener(file_path_3,
                           FileWriteMode::kReuseOrCreate,
                           FilePermissions::kWorldReadable));
  EXPECT_NE(kInvalidFileHandle, file_handle);
  EXPECT_TRUE(FileExists(file_path_3));
  EXPECT_EQ(0, FileSize(file_path_3));
}

TEST(FileIO, OpenFileForWrite) {
  TestOpenFileForWrite(OpenFileForWrite);
}

TEST(FileIO, OpenFileForReadAndWrite) {
  TestOpenFileForWrite(OpenFileForReadAndWrite);
}

TEST(FileIO, LoggingOpenFileForWrite) {
  TestOpenFileForWrite(LoggingOpenFileForWrite);
}

TEST(FileIO, LoggingOpenFileForReadAndWrite) {
  TestOpenFileForWrite(LoggingOpenFileForReadAndWrite);
}

enum class ReadOrWrite : bool {
  kRead,
  kWrite,
};

void FileShareModeTest(ReadOrWrite first, ReadOrWrite second) {
  ScopedTempDir temp_dir;
  base::FilePath shared_file =
      temp_dir.path().Append(FILE_PATH_LITERAL("shared_file"));
  {
    // Create an empty file to work on.
    ScopedFileHandle create(
        LoggingOpenFileForWrite(shared_file,
                                FileWriteMode::kCreateOrFail,
                                FilePermissions::kOwnerOnly));
  }

  auto handle1 = ScopedFileHandle(
      (first == ReadOrWrite::kRead)
          ? LoggingOpenFileForRead(shared_file)
          : LoggingOpenFileForWrite(shared_file,
                                    FileWriteMode::kReuseOrCreate,
                                    FilePermissions::kOwnerOnly));
  ASSERT_NE(handle1, kInvalidFileHandle);
  auto handle2 = ScopedFileHandle(
      (second == ReadOrWrite::kRead)
          ? LoggingOpenFileForRead(shared_file)
          : LoggingOpenFileForWrite(shared_file,
                                    FileWriteMode::kReuseOrCreate,
                                    FilePermissions::kOwnerOnly));
  EXPECT_NE(handle2, kInvalidFileHandle);

  EXPECT_NE(handle1.get(), handle2.get());
}

TEST(FileIO, FileShareMode_Read_Read) {
  FileShareModeTest(ReadOrWrite::kRead, ReadOrWrite::kRead);
}

TEST(FileIO, FileShareMode_Read_Write) {
  FileShareModeTest(ReadOrWrite::kRead, ReadOrWrite::kWrite);
}

TEST(FileIO, FileShareMode_Write_Read) {
  FileShareModeTest(ReadOrWrite::kWrite, ReadOrWrite::kRead);
}

TEST(FileIO, FileShareMode_Write_Write) {
  FileShareModeTest(ReadOrWrite::kWrite, ReadOrWrite::kWrite);
}

TEST(FileIO, MultipleSharedLocks) {
  ScopedTempDir temp_dir;
  base::FilePath shared_file =
      temp_dir.path().Append(FILE_PATH_LITERAL("file_to_lock"));

  {
    // Create an empty file to lock.
    ScopedFileHandle create(
        LoggingOpenFileForWrite(shared_file,
                                FileWriteMode::kCreateOrFail,
                                FilePermissions::kOwnerOnly));
  }

  auto handle1 = ScopedFileHandle(LoggingOpenFileForRead(shared_file));
  ASSERT_NE(handle1, kInvalidFileHandle);
  EXPECT_TRUE(LoggingLockFile(handle1.get(), FileLocking::kShared));

  auto handle2 = ScopedFileHandle(LoggingOpenFileForRead(shared_file));
  ASSERT_NE(handle1, kInvalidFileHandle);
  EXPECT_TRUE(LoggingLockFile(handle2.get(), FileLocking::kShared));

  EXPECT_TRUE(LoggingUnlockFile(handle1.get()));
  EXPECT_TRUE(LoggingUnlockFile(handle2.get()));
}

class LockingTestThread : public Thread {
 public:
  LockingTestThread()
      : file_(), lock_type_(), iterations_(), actual_iterations_() {}

  void Init(FileHandle file,
            FileLocking lock_type,
            int iterations,
            base::subtle::Atomic32* actual_iterations) {
    ASSERT_NE(file, kInvalidFileHandle);
    file_ = ScopedFileHandle(file);
    lock_type_ = lock_type;
    iterations_ = iterations;
    actual_iterations_ = actual_iterations;
  }

 private:
  void ThreadMain() override {
    for (int i = 0; i < iterations_; ++i) {
      EXPECT_TRUE(LoggingLockFile(file_.get(), lock_type_));
      base::subtle::NoBarrier_AtomicIncrement(actual_iterations_, 1);
      EXPECT_TRUE(LoggingUnlockFile(file_.get()));
    }
  }

  ScopedFileHandle file_;
  FileLocking lock_type_;
  int iterations_;
  base::subtle::Atomic32* actual_iterations_;

  DISALLOW_COPY_AND_ASSIGN(LockingTestThread);
};

void LockingTest(FileLocking main_lock, FileLocking other_locks) {
  ScopedTempDir temp_dir;
  base::FilePath shared_file =
      temp_dir.path().Append(FILE_PATH_LITERAL("file_to_lock"));

  {
    // Create an empty file to lock.
    ScopedFileHandle create(
        LoggingOpenFileForWrite(shared_file,
                                FileWriteMode::kCreateOrFail,
                                FilePermissions::kOwnerOnly));
  }

  auto initial = ScopedFileHandle(
      (main_lock == FileLocking::kShared)
          ? LoggingOpenFileForRead(shared_file)
          : LoggingOpenFileForWrite(shared_file,
                                    FileWriteMode::kReuseOrCreate,
                                    FilePermissions::kOwnerOnly));
  ASSERT_NE(initial, kInvalidFileHandle);
  ASSERT_TRUE(LoggingLockFile(initial.get(), main_lock));

  base::subtle::Atomic32 actual_iterations = 0;

  LockingTestThread threads[20];
  int expected_iterations = 0;
  for (size_t index = 0; index < arraysize(threads); ++index) {
    int iterations_for_this_thread = static_cast<int>(index * 10);
    threads[index].Init(
        (other_locks == FileLocking::kShared)
            ? LoggingOpenFileForRead(shared_file)
            : LoggingOpenFileForWrite(shared_file,
                                      FileWriteMode::kReuseOrCreate,
                                      FilePermissions::kOwnerOnly),
        other_locks,
        iterations_for_this_thread,
        &actual_iterations);
    expected_iterations += iterations_for_this_thread;

    ASSERT_NO_FATAL_FAILURE(threads[index].Start());
  }

  base::subtle::Atomic32 result =
      base::subtle::NoBarrier_Load(&actual_iterations);
  EXPECT_EQ(0, result);

  ASSERT_TRUE(LoggingUnlockFile(initial.get()));

  for (auto& t : threads)
    t.Join();

  result = base::subtle::NoBarrier_Load(&actual_iterations);
  EXPECT_EQ(expected_iterations, result);
}

TEST(FileIO, ExclusiveVsExclusives) {
  LockingTest(FileLocking::kExclusive, FileLocking::kExclusive);
}

TEST(FileIO, ExclusiveVsShareds) {
  LockingTest(FileLocking::kExclusive, FileLocking::kShared);
}

TEST(FileIO, SharedVsExclusives) {
  LockingTest(FileLocking::kShared, FileLocking::kExclusive);
}

}  // namespace
}  // namespace test
}  // namespace crashpad
