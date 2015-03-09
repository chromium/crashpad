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

#ifndef CRASHPAD_CLIENT_SETTINGS_H_
#define CRASHPAD_CLIENT_SETTINGS_H_

#include <time.h>

#include <string>

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "util/file/file_io.h"
#include "util/misc/initialization_state_dcheck.h"
#include "util/misc/uuid.h"

namespace crashpad {

//! \brief An interface for accessing and modifying the settings of a
//!     CrashReportDatabase.
//!
//! This class must not be instantiated directly, but rather an instance of it
//! should be retrieved via CrashReportDatabase::GetSettings().
class Settings {
 public:
  explicit Settings(const base::FilePath& file_path);
  ~Settings();

  bool Initialize();

  //! \brief Retrieves the immutable identifier for this client, which is used
  //!     on a server to locate all crash reports from a specific Crashpad
  //!     database.
  //!
  //! This is automatically initialized when the database is created.
  //!
  //! \param[out] client_id The unique client identifier.
  //!
  //! \return On success, returns `true`, otherwise returns `false` with an
  //!     error logged.
  bool GetClientID(UUID* client_id);

  //! \brief Retrieves the user’s preference for submitting crash reports to a
  //!     collection server.
  //!
  //! The default value is `false`.
  //!
  //! \param[out] enabled Whether crash reports should be uploaded.
  //!
  //! \return On success, returns `true`, otherwise returns `false` with an
  //!     error logged.
  bool GetUploadsEnabled(bool* enabled);

  //! \brief Sets the user’s preference for submitting crash reports to a
  //!     collection server.
  //!
  //! \param[in] enabled Whether crash reports should be uploaded.
  //!
  //! \return On success, returns `true`, otherwise returns `false` with an
  //!     error logged.
  bool SetUploadsEnabled(bool enabled);

  //! \brief Retrieves the last time at which a report was attempted to be
  //!     uploaded.
  //!
  //! The default value is `0` if it has never been set before.
  //!
  //! \param[out] time The last time at which a report was uploaded.
  //!
  //! \return On success, returns `true`, otherwise returns `false` with an
  //!     error logged.
  bool GetLastUploadAttemptTime(time_t* time);

  //! \brief Sets the last time at which a report was attempted to be uploaded.
  //!
  //! This is only meant to be used internally by the CrashReportDatabase.
  //!
  //! \param[in] time The last time at which a report was uploaded.
  //!
  //! \return On success, returns `true`, otherwise returns `false` with an
  //!     error logged.
  bool SetLastUploadAttemptTime(time_t time);

 private:
  struct Data;

  // Opens the settings file for reading. On error, logs a message and returns
  // the invalid handle.
  ScopedFileHandle OpenForReading();

  // Opens the settings file for reading and writing. On error, logs a message
  // and returns the invalid handle.
  ScopedFileHandle OpenForReadingAndWriting();

  // Opens the settings file and reads the data. If that fails, an error will
  // be logged and the settings will be recovered and re-initialized. If that
  // also fails, returns false with additional log data from recovery.
  bool OpenAndReadSettings(Data* out_data);

  // Opens the settings file for writing and reads the data. If reading fails,
  // recovery is attempted. Returns the opened file handle on success, or the
  // invalid file handle on failure, with an error logged.
  ScopedFileHandle OpenForWritingAndReadSettings(Data* out_data);

  // Reads the settings from |handle|. Logs an error and returns false on
  // failure. This does not perform recovery.
  bool ReadSettings(FileHandle handle, Data* out_data);

  // Writes the settings to |handle|. Logs an error and returns false on
  // failure. This does not perform recovery.
  bool WriteSettings(FileHandle handle, const Data& data);

  // Recovers the settings file by re-initializing the data. If |handle| is the
  // invalid handle, this will open the file; if it is not, then it must be the
  // result of OpenForReadingAndWriting(). If the invalid handle is passed, the
  // caller must not be holding the handle. The new settings data are stored in
  // |out_data|. Returns true on success and false on failure, with an error
  // logged.
  bool RecoverSettings(FileHandle handle, Data* out_data);

  // Initializes a settings file and writes the data to |handle|. Returns true
  // on success and false on failure, with an error logged.
  bool InitializeSettings(FileHandle handle);

  const char* file_path() { return file_path_.value().c_str(); }

  base::FilePath file_path_;

  InitializationStateDcheck initialized_;

  DISALLOW_COPY_AND_ASSIGN(Settings);
};

}  // namespace crashpad

#endif  // CRASHPAD_CLIENT_SETTINGS_H_
