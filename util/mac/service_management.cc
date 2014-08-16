// Copyright 2014 The Crashpad Authors. All rights reserved.
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

#include "util/mac/service_management.h"

#include <errno.h>
#include <launch.h>
#include <time.h>

#include "base/mac/scoped_launch_data.h"
#include "util/mac/launchd.h"

namespace {

launch_data_t LaunchDataDictionaryForJob(const std::string& label) {
  base::mac::ScopedLaunchData request(
      launch_data_alloc(LAUNCH_DATA_DICTIONARY));
  launch_data_dict_insert(
      request, launch_data_new_string(label.c_str()), LAUNCH_KEY_GETJOB);

  base::mac::ScopedLaunchData response(launch_msg(request));
  if (launch_data_get_type(response) != LAUNCH_DATA_DICTIONARY) {
    return NULL;
  }

  return response.release();
}

}  // namespace

namespace crashpad {

bool ServiceManagementSubmitJob(CFDictionaryRef job_cf) {
  base::mac::ScopedLaunchData job_launch(CFPropertyToLaunchData(job_cf));
  if (!job_launch.get()) {
    return false;
  }

  base::mac::ScopedLaunchData jobs(launch_data_alloc(LAUNCH_DATA_ARRAY));
  launch_data_array_set_index(jobs, job_launch.release(), 0);

  base::mac::ScopedLaunchData request(
      launch_data_alloc(LAUNCH_DATA_DICTIONARY));
  launch_data_dict_insert(request, jobs.release(), LAUNCH_KEY_SUBMITJOB);

  base::mac::ScopedLaunchData response(launch_msg(request));

  if (launch_data_get_type(response) != LAUNCH_DATA_ARRAY) {
    return false;
  }

  if (launch_data_array_get_count(response) != 1) {
    return false;
  }

  launch_data_t response_element = launch_data_array_get_index(response, 0);
  if (launch_data_get_type(response_element) != LAUNCH_DATA_ERRNO) {
    return false;
  }

  int err = launch_data_get_errno(response_element);
  if (err != 0) {
    return false;
  }

  return true;
}

bool ServiceManagementRemoveJob(const std::string& label, bool wait) {
  base::mac::ScopedLaunchData request(
      launch_data_alloc(LAUNCH_DATA_DICTIONARY));
  launch_data_dict_insert(
      request, launch_data_new_string(label.c_str()), LAUNCH_KEY_REMOVEJOB);

  base::mac::ScopedLaunchData response(launch_msg(request));
  if (launch_data_get_type(response) != LAUNCH_DATA_ERRNO) {
    return false;
  }

  int err = launch_data_get_errno(response);
  if (err == EINPROGRESS) {
    if (wait) {
      // TODO(mark): Use a kqueue to wait for the process to exit. To avoid a
      // race, the kqueue would need to be set up prior to asking launchd to
      // remove the job. Even so, the job’s PID may change between the time it’s
      // obtained and the time the kqueue is set up, so this is nontrivial.
      do {
        timespec sleep_time;
        sleep_time.tv_sec = 0;
        sleep_time.tv_nsec = 1E5;  // 100 microseconds
        nanosleep(&sleep_time, NULL);
      } while (ServiceManagementIsJobLoaded(label));
    }

    return true;
  }

  if (err != 0) {
    return false;
  }

  return true;
}

bool ServiceManagementIsJobLoaded(const std::string& label) {
  base::mac::ScopedLaunchData dictionary(LaunchDataDictionaryForJob(label));
  if (!dictionary) {
    return false;
  }

  return true;
}

pid_t ServiceManagementIsJobRunning(const std::string& label) {
  base::mac::ScopedLaunchData dictionary(LaunchDataDictionaryForJob(label));
  if (!dictionary) {
    return 0;
  }

  launch_data_t pid = launch_data_dict_lookup(dictionary, LAUNCH_JOBKEY_PID);
  if (launch_data_get_type(pid) != LAUNCH_DATA_INTEGER) {
    return 0;
  }

  return launch_data_get_integer(pid);
}

}  // namespace crashpad
