// Copyright 2015 The Crashpad Authors
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

#include "client/settings.h"

#include <stdint.h>
#include <string.h>

#include <limits>

#include "base/logging.h"
#include "base/notreached.h"
#include "base/posix/eintr_wrapper.h"
#include "build/build_config.h"
#include "util/file/filesystem.h"
#include "util/numeric/in_range_cast.h"

namespace crashpad {

#if !CRASHPAD_FLOCK_ALWAYS_SUPPORTED

Settings::ScopedLockedFileHandle::ScopedLockedFileHandle()
    : handle_(kInvalidFileHandle), lockfile_path_() {
    }

Settings::ScopedLockedFileHandle::ScopedLockedFileHandle(
    FileHandle handle,
    const base::FilePath& lockfile_path)
    : handle_(handle), lockfile_path_(lockfile_path) {
}

Settings::ScopedLockedFileHandle::ScopedLockedFileHandle(
    ScopedLockedFileHandle&& other)
    : handle_(other.handle_), lockfile_path_(other.lockfile_path_) {
  other.handle_ = kInvalidFileHandle;
  other.lockfile_path_ = base::FilePath();
}

Settings::ScopedLockedFileHandle& Settings::ScopedLockedFileHandle::operator=(
    ScopedLockedFileHandle&& other) {
  handle_ = other.handle_;
  lockfile_path_ = other.lockfile_path_;

  other.handle_ = kInvalidFileHandle;
  other.lockfile_path_ = base::FilePath();
  return *this;
}

Settings::ScopedLockedFileHandle::~ScopedLockedFileHandle() {
  Destroy();
}

void Settings::ScopedLockedFileHandle::Destroy() {
  if (handle_ != kInvalidFileHandle) {
    CheckedCloseFile(handle_);
  }
  if (!lockfile_path_.empty()) {
    const bool success = LoggingRemoveFile(lockfile_path_);
    DCHECK(success);
  }
}

#else  // !CRASHPAD_FLOCK_ALWAYS_SUPPORTED

#if BUILDFLAG(IS_IOS)

Settings::ScopedLockedFileHandle::ScopedLockedFileHandle(
    const FileHandle& value)
    : ScopedGeneric(value) {
  ios_background_task_ = std::make_unique<internal::ScopedBackgroundTask>(
      "ScopedLockedFileHandle");
}

Settings::ScopedLockedFileHandle::ScopedLockedFileHandle(
    Settings::ScopedLockedFileHandle&& rvalue) {
  ios_background_task_.reset(rvalue.ios_background_task_.release());
  reset(rvalue.release());
}

Settings::ScopedLockedFileHandle& Settings::ScopedLockedFileHandle::operator=(
    Settings::ScopedLockedFileHandle&& rvalue) {
  ios_background_task_.reset(rvalue.ios_background_task_.release());
  reset(rvalue.release());
  return *this;
}

Settings::ScopedLockedFileHandle::~ScopedLockedFileHandle() {
  // Call reset() to ensure the lock is released before the ios_background_task.
  reset();
}

#endif  // BUILDFLAG(IS_IOS)

namespace internal {

// static
void ScopedLockedFileHandleTraits::Free(FileHandle handle) {
  if (handle != kInvalidFileHandle) {
    LoggingUnlockFile(handle);
    CheckedCloseFile(handle);
  }
}

}  // namespace internal

#endif  // BUILDFLAG(IS_FUCHSIA)

struct Settings::Data {
  static constexpr uint32_t kSettingsMagic = 'CPds';

  // Version number only used for incompatible changes to Data. Do not change
  // this when adding additional fields at the end. Modifying `kSettingsVersion`
  // will wipe away the entire struct when reading from other versions.
  static constexpr uint32_t kSettingsVersion = 1;

  enum Options : uint32_t {
    kUploadsEnabled = 1 << 0,
  };

  Data() : magic(kSettingsMagic),
           version(kSettingsVersion),
           options(0),
           padding_0(0),
           last_upload_attempt_time(0),
           client_id() {}

  uint32_t magic;
  uint32_t version;
  uint32_t options;
  uint32_t padding_0;
  int64_t last_upload_attempt_time;  // time_t
  UUID client_id;
};

Settings::Settings() = default;

Settings::~Settings() = default;

bool Settings::Initialize(const base::FilePath& file_path) {
  DCHECK(initialized_.is_uninitialized());
  initialized_.set_invalid();
  file_path_ = file_path;

  Data settings;
  if (!OpenForWritingAndReadSettings(&settings).is_valid())
    return false;

  initialized_.set_valid();
  return true;
}

bool Settings::GetClientID(UUID* client_id) {
  DCHECK(initialized_.is_valid());

  Data settings;
  if (!OpenAndReadSettings(&settings))
    return false;

  *client_id = settings.client_id;
  return true;
}

bool Settings::GetUploadsEnabled(bool* enabled) {
  DCHECK(initialized_.is_valid());

  Data settings;
  if (!OpenAndReadSettings(&settings))
    return false;

  *enabled = (settings.options & Data::Options::kUploadsEnabled) != 0;
  return true;
}

bool Settings::SetUploadsEnabled(bool enabled) {
  DCHECK(initialized_.is_valid());

  Data settings;
  ScopedLockedFileHandle handle = OpenForWritingAndReadSettings(&settings);
  if (!handle.is_valid())
    return false;

  if (enabled)
    settings.options |= Data::Options::kUploadsEnabled;
  else
    settings.options &= ~Data::Options::kUploadsEnabled;

  return WriteSettings(handle.get(), settings);
}

bool Settings::GetLastUploadAttemptTime(time_t* time) {
  DCHECK(initialized_.is_valid());

  Data settings;
  if (!OpenAndReadSettings(&settings))
    return false;

  *time = InRangeCast<time_t>(settings.last_upload_attempt_time,
                              std::numeric_limits<time_t>::max());
  return true;
}

bool Settings::SetLastUploadAttemptTime(time_t time) {
  DCHECK(initialized_.is_valid());

  Data settings;
  ScopedLockedFileHandle handle = OpenForWritingAndReadSettings(&settings);
  if (!handle.is_valid())
    return false;

  settings.last_upload_attempt_time = InRangeCast<int64_t>(time, 0);

  return WriteSettings(handle.get(), settings);
}

#if !CRASHPAD_FLOCK_ALWAYS_SUPPORTED
// static
bool Settings::IsLockExpired(const base::FilePath& file_path,
                             time_t lockfile_ttl) {
  time_t now = time(nullptr);
  base::FilePath lock_path(file_path.value() + Settings::kLockfileExtension);
  ScopedFileHandle lock_fd(LoggingOpenFileForRead(lock_path));
  time_t lock_timestamp;
  if (!LoggingReadFileExactly(
          lock_fd.get(), &lock_timestamp, sizeof(lock_timestamp))) {
    return false;
  }
  return now >= lock_timestamp + lockfile_ttl;
}
#endif  // !CRASHPAD_FLOCK_ALWAYS_SUPPORTED

// static
Settings::ScopedLockedFileHandle Settings::MakeScopedLockedFileHandle(
    const internal::MakeScopedLockedFileHandleOptions& options,
    FileLocking locking,
    const base::FilePath& file_path) {
#if !CRASHPAD_FLOCK_ALWAYS_SUPPORTED
  base::FilePath lockfile_path(file_path.value() +
                               Settings::kLockfileExtension);
  ScopedFileHandle lockfile_scoped(
      LoggingOpenFileForWrite(lockfile_path,
                              FileWriteMode::kCreateOrFail,
                              FilePermissions::kWorldReadable));
  if (!lockfile_scoped.is_valid()) {
    return ScopedLockedFileHandle();
  }
  time_t now = time(nullptr);
  if (!LoggingWriteFile(lockfile_scoped.get(), &now, sizeof(now))) {
    return ScopedLockedFileHandle();
  }
  ScopedFileHandle scoped(GetHandleFromOptions(file_path, options));
  if (scoped.is_valid()) {
    return ScopedLockedFileHandle(scoped.release(), lockfile_path);
  }
  bool success = LoggingRemoveFile(lockfile_path);
  DCHECK(success);
  return ScopedLockedFileHandle();
#else
  ScopedFileHandle scoped(GetHandleFromOptions(file_path, options));
  // It's important to create the ScopedLockedFileHandle before calling
  // LoggingLockFile on iOS, so a ScopedBackgroundTask is created *before*
  // the LoggingLockFile call below.
  ScopedLockedFileHandle handle(kInvalidFileHandle);
  if (scoped.is_valid()) {
    if (LoggingLockFile(
            scoped.get(), locking, FileLockingBlocking::kBlocking) !=
        FileLockingResult::kSuccess) {
      scoped.reset();
    }
  }
  handle.reset(scoped.release());
  return handle;
#endif  // !CRASHPAD_FLOCK_ALWAYS_SUPPORTED
}

// static
FileHandle Settings::GetHandleFromOptions(
    const base::FilePath& file_path,
    const internal::MakeScopedLockedFileHandleOptions& options) {
  switch (options.function_enum) {
    case internal::FileOpenFunction::kLoggingOpenFileForRead:
      return LoggingOpenFileForRead(file_path);
    case internal::FileOpenFunction::kLoggingOpenFileForReadAndWrite:
      return LoggingOpenFileForReadAndWrite(
          file_path, options.mode, options.permissions);
    case internal::FileOpenFunction::kOpenFileForReadAndWrite:
      return OpenFileForReadAndWrite(
          file_path, options.mode, options.permissions);
  }
  NOTREACHED();
}

Settings::ScopedLockedFileHandle Settings::OpenForReading() {
  internal::MakeScopedLockedFileHandleOptions options;
  options.function_enum = internal::FileOpenFunction::kLoggingOpenFileForRead;
  return MakeScopedLockedFileHandle(options, FileLocking::kShared, file_path());
}

Settings::ScopedLockedFileHandle Settings::OpenForReadingAndWriting(
    FileWriteMode mode, bool log_open_error) {
  DCHECK(mode != FileWriteMode::kTruncateOrCreate);

  internal::MakeScopedLockedFileHandleOptions options;
  options.mode = mode;
  options.permissions = FilePermissions::kOwnerOnly;
  if (log_open_error) {
    options.function_enum =
        internal::FileOpenFunction::kLoggingOpenFileForReadAndWrite;
  } else {
    options.function_enum =
        internal::FileOpenFunction::kOpenFileForReadAndWrite;
  }

  return MakeScopedLockedFileHandle(
      options, FileLocking::kExclusive, file_path());
}

bool Settings::OpenAndReadSettings(Data* out_data) {
  ScopedLockedFileHandle handle = OpenForReading();
  if (!handle.is_valid())
    return false;

  if (ReadSettings(handle.get(), out_data, true))
    return true;

  // The settings file is corrupt, so reinitialize it.
  handle.reset();

  // The settings failed to be read, so re-initialize them.
  return RecoverSettings(kInvalidFileHandle, out_data);
}

Settings::ScopedLockedFileHandle Settings::OpenForWritingAndReadSettings(
    Data* out_data) {
  ScopedLockedFileHandle handle;
  bool created = false;
  if (!initialized_.is_valid()) {
    // If this object is initializing, it hasn’t seen a settings file already,
    // so go easy on errors. Creating a new settings file for the first time
    // shouldn’t spew log messages.
    //
    // First, try to use an existing settings file.
    handle = OpenForReadingAndWriting(FileWriteMode::kReuseOrFail, false);

    if (!handle.is_valid()) {
      // Create a new settings file if it didn’t already exist.
      handle = OpenForReadingAndWriting(FileWriteMode::kCreateOrFail, false);

      if (handle.is_valid()) {
        created = true;
      }

      // There may have been a race to create the file, and something else may
      // have won. There will be one more attempt to try to open or create the
      // file below.
    }
  }

  if (!handle.is_valid()) {
    // Either the object is initialized, meaning it’s already seen a valid
    // settings file, or the object is initializing and none of the above
    // attempts to create the settings file succeeded. Either way, this is the
    // last chance for success, so if this fails, log a message.
    handle = OpenForReadingAndWriting(FileWriteMode::kReuseOrCreate, true);
  }

  if (!handle.is_valid())
    return ScopedLockedFileHandle();

  // Attempt reading the settings even if the file is known to have just been
  // created. The file-create and file-lock operations don’t occur atomically,
  // and something else may have written the settings before this invocation
  // took the lock. If the settings file was definitely just created, though,
  // don’t log any read errors. The expected non-race behavior in this case is a
  // zero-length read, with ReadSettings() failing.
  if (!ReadSettings(handle.get(), out_data, !created)) {
    if (!RecoverSettings(handle.get(), out_data))
      return ScopedLockedFileHandle();
  }

  return handle;
}

bool Settings::ReadSettings(FileHandle handle,
                            Data* out_data,
                            bool log_read_error) {
  if (LoggingSeekFile(handle, 0, SEEK_SET) != 0) {
    return false;
  }

  // This clears `out_data` so that any bytes not read from disk are zero
  // initialized. This is expected when reading from an older settings file with
  // fewer fields.
  memset(out_data, 0, sizeof(*out_data));

  const FileOperationResult read_result =
      log_read_error ? LoggingReadFileUntil(handle, out_data, sizeof(*out_data))
                     : ReadFileUntil(handle, out_data, sizeof(*out_data));

  if (read_result <= 0) {
    return false;
  }

  // Newer versions of crashpad may add fields to Data, but all versions have
  // the data members up to `client_id`. Do not attempt to understand a smaller
  // struct read.
  const size_t min_size =
      offsetof(Data, client_id) + sizeof(out_data->client_id);
  if (static_cast<size_t>(read_result) < min_size) {
    LOG(ERROR) << "Settings file too small: minimum " << min_size
               << ", observed " << read_result;
    return false;
  }

  if (out_data->magic != Data::kSettingsMagic) {
    LOG(ERROR) << "Settings magic is not " << Data::kSettingsMagic;
    return false;
  }

  if (out_data->version != Data::kSettingsVersion) {
    LOG(ERROR) << "Settings version is not " << Data::kSettingsVersion;
    return false;
  }

  return true;
}

bool Settings::WriteSettings(FileHandle handle, const Data& data) {
  if (LoggingSeekFile(handle, 0, SEEK_SET) != 0)
    return false;

  if (!LoggingTruncateFile(handle))
    return false;

  return LoggingWriteFile(handle, &data, sizeof(Data));
}

bool Settings::RecoverSettings(FileHandle handle, Data* out_data) {
  ScopedLockedFileHandle scoped_handle;
  if (handle == kInvalidFileHandle) {
    scoped_handle =
        OpenForReadingAndWriting(FileWriteMode::kReuseOrCreate, true);
    handle = scoped_handle.get();

    // Test if the file has already been recovered now that the exclusive lock
    // is held.
    if (ReadSettings(handle, out_data, true))
      return true;
  }

  if (handle == kInvalidFileHandle) {
    LOG(ERROR) << "Invalid file handle";
    return false;
  }

  if (!InitializeSettings(handle))
    return false;

  return ReadSettings(handle, out_data, true);
}

bool Settings::InitializeSettings(FileHandle handle) {
  Data settings;
  if (!settings.client_id.InitializeWithNew())
    return false;

  return WriteSettings(handle, settings);
}

}  // namespace crashpad
