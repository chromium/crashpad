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
constexpr base::FilePath::CharType kSettingsStoreInitialized[] =
    FILE_PATH_LITERAL("settings_store_initialized");

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
    /*
    if (fsync(temp.get() < 0)) {
      PLOG(ERROR) << "fsync";
      return false;
    }
    */
  }

  // TODO(scottmg): This is rename() on Fuchsia (and POSIX), but confirm if
  // MoveFileEx() is atomic before using on Windows.
  if (!MoveFileOrDirectory(temp_name, file_path)) {
    return false;
  }

  return true;
}

template <class T>
bool ReadFrom(const base::FilePath& file_path, T* data) {
  ScopedFileHandle file(OpenFileForRead(file_path));
  return LoggingReadFileExactly(file.get(), data, sizeof(*data));
}

}  // namespace

Settings::Settings() = default;

Settings::~Settings() = default;

bool Settings::Initialize(const base::FilePath& file_path) {
  initialized_.set_invalid();
  file_path_ = file_path;

  // If non-reusing creation succeeds, this is a new settings store, and the
  // default data must be initialized. If it fails, then another process is
  // currently creating the database. Confirm with retry that the other process
  // has created the client id, by retrieving it.
  if (LoggingCreateDirectory(file_path_, FilePermissions::kOwnerOnly, false)) {
    // Create a client ID, and initialize other data to default values.
    UUID new_client_id;
    if (!new_client_id.InitializeWithNew()) {
      LOG(ERROR) << "client id creation failed";
      return false;
    }
    if (!WriteAndRename(file_path_.Append(kClientId), new_client_id)) {
      LOG(ERROR) << "client id creation write";
      return false;
    }

    initialized_.set_valid();
    SetUploadsEnabled(0);
    SetLastUploadAttemptTime(0);

    // Finish by creating an empty file signaling that the data store has been
    // completely initialized. This is only used to handle simultaneous creators
    // of the settings store racing between the winner that gets to initialize,
    // and the loser that must wait until this file is created.
    ScopedFileHandle creation_complete(
        OpenFileForWrite(file_path_.Append(kSettingsStoreInitialized),
                         FileWriteMode::kCreateOrFail,
                         FilePermissions::kOwnerOnly));
  } else {
    for (int retries = 0; ; ++retries) {
      // If the store has been initialized, all is well.
      if (IsRegularFile(file_path_.Append(kSettingsStoreInitialized))) {
        break;
      }

      // Otherwise, wait on the other process that is in the process of
      // initializing it. This should be very unlikely to execute.
      LOG(ERROR) << "warning: waiting for other process to initialize settings";
      SleepNanoseconds(1e9);

      if (retries == 10) {
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
  return ReadFrom(file_path_.Append(kClientId), client_id);
}

bool Settings::GetUploadsEnabled(bool* enabled) {
  DCHECK(initialized_.is_valid());
  return ReadFrom(file_path_.Append(kUploadsEnabled), enabled);
}

bool Settings::SetUploadsEnabled(bool enabled) {
  DCHECK(initialized_.is_valid());
  return WriteAndRename(file_path_.Append(kUploadsEnabled), enabled);
}

bool Settings::GetLastUploadAttemptTime(time_t* time) {
  DCHECK(initialized_.is_valid());
  return ReadFrom(file_path_.Append(kLastUploadAttemptTime), time);
}

bool Settings::SetLastUploadAttemptTime(time_t time) {
  DCHECK(initialized_.is_valid());
  return WriteAndRename(file_path_.Append(kLastUploadAttemptTime), time);
}

}  // namespace crashpad
