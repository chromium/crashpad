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

#include <launch.h>
#include <ServiceManagement/ServiceManagement.h>

#include "base/mac/foundation_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/strings/sys_string_conversions.h"

// ServiceManagement.framework is available on 10.6 and later, but it’s
// deprecated in 10.10. In case ServiceManagement.framework stops working in the
// future, an alternative implementation using launch_msg() is available. This
// implementation works on 10.5 and later, however, launch_msg() is also
// deprecated in 10.10. The alternative implementation can be resurrected from
// source control history.

namespace {

// Wraps the necessary functions from ServiceManagement.framework to avoid the
// deprecation warnings when using the 10.10 SDK.

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

Boolean CallSMJobSubmit(CFStringRef domain,
                        CFDictionaryRef job,
                        AuthorizationRef authorization,
                        CFErrorRef *error) {
  return SMJobSubmit(domain, job, authorization, error);
}

Boolean CallSMJobRemove(CFStringRef domain,
                        CFStringRef job_label,
                        AuthorizationRef authorization,
                        Boolean wait,
                        CFErrorRef *error) {
  return SMJobRemove(domain, job_label, authorization, wait, error);
}

CFDictionaryRef CallSMJobCopyDictionary(
    CFStringRef domain, CFStringRef job_label) {
  return SMJobCopyDictionary(domain, job_label);
}

#pragma GCC diagnostic pop

}  // namespace

namespace crashpad {

bool ServiceManagementSubmitJob(CFDictionaryRef job_cf) {
  return CallSMJobSubmit(kSMDomainUserLaunchd, job_cf, NULL, NULL);
}

bool ServiceManagementRemoveJob(const std::string& label, bool wait) {
  base::ScopedCFTypeRef<CFStringRef> label_cf(
      base::SysUTF8ToCFStringRef(label));
  return CallSMJobRemove(kSMDomainUserLaunchd, label_cf, NULL, wait, NULL);
}

bool ServiceManagementIsJobLoaded(const std::string& label) {
  base::ScopedCFTypeRef<CFStringRef> label_cf(
      base::SysUTF8ToCFStringRef(label));
  base::ScopedCFTypeRef<CFDictionaryRef> job_dictionary(
      CallSMJobCopyDictionary(kSMDomainUserLaunchd, label_cf));
  return job_dictionary != NULL;
}

pid_t ServiceManagementIsJobRunning(const std::string& label) {
  base::ScopedCFTypeRef<CFStringRef> label_cf(
      base::SysUTF8ToCFStringRef(label));
  base::ScopedCFTypeRef<CFDictionaryRef> job_dictionary(
      CallSMJobCopyDictionary(kSMDomainUserLaunchd, label_cf));
  if (job_dictionary != NULL) {
    CFNumberRef pid_cf = base::mac::CFCast<CFNumberRef>(
        CFDictionaryGetValue(job_dictionary, CFSTR(LAUNCH_JOBKEY_PID)));
    if (pid_cf) {
      pid_t pid;
      if (CFNumberGetValue(pid_cf, kCFNumberIntType, &pid)) {
        return pid;
      }
    }
  }
  return 0;
}

}  // namespace crashpad
