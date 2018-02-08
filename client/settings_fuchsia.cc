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

#include "client/settings.h"

#include <dirent.h>
#include <fdio/watcher.h>
#include <stdint.h>
#include <unistd.h>

#include "base/logging.h"
#include "util/file/filesystem.h"
#include "util/misc/clock.h"

namespace crashpad {

namespace {

constexpr base::FilePath::CharType kClientId[] =
    FILE_PATH_LITERAL("client_id");
constexpr base::FilePath::CharType kUploadsEnabled[] =
    FILE_PATH_LITERAL("uploads_enabled");
constexpr base::FilePath::CharType kLastUploadAttemptTime[] =
    FILE_PATH_LITERAL("last_upload_attempt_time");

// Write data to a temporary location next to file_path and atomically
// rename the temporary location to file_path.
template <class T>
bool WriteAndRename(const base::FilePath& file_path, const T& data) {
  UUID temp_extension;
  temp_extension.InitializeWithNew();
  base::FilePath temp_name(file_path.value() + "." + temp_extension.ToString());

  {
    ScopedFileHandle temp(OpenFileForWrite(
        temp_name, FileWriteMode::kCreateOrFail, FilePermissions::kOwnerOnly));
    if (!LoggingWriteFile(temp.get(), &data, sizeof(data))) {
      return false;
    }

    // TODO(scottmg): Is fsync() required? Sufficient?
    if (fsync(temp.get()) < 0) {
      PLOG(ERROR) << "fsync";
      return false;
    }
  }

  // TODO(scottmg): This is rename() on Fuchsia (and POSIX), but confirm if
  // MoveFileEx() is atomic before using on Windows.
  if (!MoveFileOrDirectory(temp_name, file_path)) {
    return false;
  }

  return true;
}

void RemoveOldData(const base::FilePath& file_path) {
  if (IsRegularFile(file_path)) {
    LoggingRemoveFile(file_path);
  } else if (IsDirectory(file_path, false)) {
    // Only remove files known to the settings database.
    LoggingRemoveFile(file_path.Append(kClientId));
    LoggingRemoveFile(file_path.Append(kUploadsEnabled));
    LoggingRemoveFile(file_path.Append(kLastUploadAttemptTime));
    LoggingRemoveDirectory(file_path);
  }
}

template <class T>
bool WriteAndRenameAndReinitializeOnFailure(
    const base::FilePath& settings_root,
    const base::FilePath::CharType filename[],
    Settings* settings,
    const T& data) {
  base::FilePath file_path(settings_root.Append(filename));
  if (WriteAndRename(file_path, data))
    return true;

  // Write failed, try reinitializing the database and retrying once.
  RemoveOldData(settings_root);
  settings->Initialize(settings_root);

  return WriteAndRename(file_path, data);
}

template <class T>
bool ReadFromAndReinitializeOnFailure(const base::FilePath& settings_root,
                                      const base::FilePath::CharType filename[],
                                      Settings* settings,
                                      T* data) {
  base::FilePath file_path(settings_root.Append(filename));
  {
    ScopedFileHandle file(OpenFileForRead(file_path));
    if (LoggingReadFileExactly(file.get(), data, sizeof(*data)))
      return true;
  }

  // Read failed, try reinitializing the database and retrying once.
  LOG(ERROR) << "read of " << filename
             << " failed, attempting reinitialization";
  RemoveOldData(settings_root);
  settings->Initialize(settings_root);

  ScopedFileHandle file_retry(OpenFileForRead(file_path));
  return LoggingReadFileExactly(file_retry.get(), data, sizeof(*data));
}

bool GenerateClientID(const base::FilePath& filename) {
  UUID new_client_id;
  if (!new_client_id.InitializeWithNew()) {
    LOG(ERROR) << "client id creation failed";
    return false;
  }

  if (!WriteAndRename(filename, new_client_id)) {
    LOG(ERROR) << "client id creation write";
    return false;
  }

  return true;
}

zx_status_t WaitUntilFileCreatedCallback(int dirfd,
                                         int event,
                                         const char* fn,
                                         void* cookie) {
  if (event != WATCH_EVENT_ADD_FILE) {
    // Continue, it's not a file addition.
    return ZX_OK;
  }

  base::FilePath* look_for = reinterpret_cast<base::FilePath*>(cookie);
  if (*look_for != base::FilePath(fn)) {
    // Continue, it's not the file being waited on.
    return ZX_OK;
  }

  // Found, stop the directory watch.
  return ZX_ERR_STOP;
}

}  // namespace

Settings::Settings() = default;

Settings::~Settings() = default;

bool Settings::Initialize(const base::FilePath& file_path) {
  initialized_.set_invalid();
  file_path_ = file_path;

  if (IsRegularFile(file_path)) {
    LoggingRemoveDirectory(file_path);
  }

  // If non-reusing creation succeeds, this is a new settings store, and the
  // default data must be initialized. If it fails, then another process is
  // currently creating the database. Confirm with retry that the other process
  // has created the client id, by retrieving it. // TODO(scottmg):
  // Non-logging mkdir here, since failure is to be expected.
  if (LoggingCreateDirectory(file_path_, FilePermissions::kOwnerOnly, false)) {
    initialized_.set_valid();

    // Initialize all data. It is important that the client id be initialized
    // last, as that's what the race-loser path below waits on.
    if (!SetUploadsEnabled(false) || !SetLastUploadAttemptTime(0) ||
        !GenerateClientID(file_path_.Append(kClientId))) {
      LOG(ERROR) << "settings initialization failed";
      LoggingRemoveDirectory(file_path_);
      return false;
    }
  } else {
    // This path potentially lost a directory creation race. If the client id
    // hasn't already been created, wait until it has been, which indicates
    // initialization is complete.
    base::FilePath client_id(file_path_.Append(kClientId));
    if (!IsRegularFile(client_id)) {
      // TODO(scottmg): Non-Fuchsia-specific way of doing this.
      DIR* dir = opendir(file_path_.value().c_str());
      zx_status_t status = fdio_watch_directory(dirfd(dir),
                                                WaitUntilFileCreatedCallback,
                                                ZX_TIME_INFINITE,
                                                &client_id);
      closedir(dir);
      if (status != ZX_ERR_STOP) {
        LOG(ERROR) << "failed waiting for other process to initialize settings";
        return false;
      }
    }
  }

  initialized_.set_valid();
  return true;
}

bool Settings::GetClientID(UUID* client_id) {
  DCHECK(initialized_.is_valid());
  return ReadFromAndReinitializeOnFailure(
      file_path_, kClientId, this, client_id);
}

bool Settings::GetUploadsEnabled(bool* enabled) {
  DCHECK(initialized_.is_valid());
  return ReadFromAndReinitializeOnFailure(
      file_path_, kUploadsEnabled, this, enabled);
}

bool Settings::SetUploadsEnabled(bool enabled) {
  DCHECK(initialized_.is_valid());
  return WriteAndRenameAndReinitializeOnFailure(
      file_path_, kUploadsEnabled, this, enabled);
}

bool Settings::GetLastUploadAttemptTime(time_t* time) {
  DCHECK(initialized_.is_valid());
  return ReadFromAndReinitializeOnFailure(
      file_path_, kLastUploadAttemptTime, this, time);
}

bool Settings::SetLastUploadAttemptTime(time_t time) {
  DCHECK(initialized_.is_valid());
  return WriteAndRenameAndReinitializeOnFailure(
      file_path_, kLastUploadAttemptTime, this, time);
}

}  // namespace crashpad
