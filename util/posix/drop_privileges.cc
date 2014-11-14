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

#include <unistd.h>

#include "base/logging.h"
#include "build/build_config.h"

namespace crashpad {

void DropPrivileges() {
  gid_t gid = getgid();
  uid_t uid = getuid();

#if defined(OS_MACOSX)
  // Based on the POSIX.1-2008 2013 edition documentation for setreuid() and
  // setregid(), setreuid() and setregid() alone should be sufficient to drop
  // privileges. The standard specifies that the saved ID should be set to the
  // effective ID whenever the real ID is not -1, and whenever the effective ID
  // is set not equal to the real ID. This code never specifies -1, so the
  // setreuid() and setregid() alone should work according to the standard.
  //
  // In practice, on Mac OS X, setuid() and setgid() (or seteuid() and
  // setegid()) must be called first, otherwise, setreuid() and setregid() do
  // not alter the saved IDs, leaving open the possibility for future privilege
  // escalation. This bug is filed as radar 18987552.
  gid_t egid = getegid();
  PCHECK(setgid(gid) == 0) << "setgid";
  PCHECK(setregid(gid, gid) == 0) << "setregid";

  uid_t euid = geteuid();
  PCHECK(setuid(uid) == 0) << "setuid";
  PCHECK(setreuid(uid, uid) == 0) << "setreuid";

  if (uid != 0) {
    // Because the setXid()+setreXid() interface to change IDs is fragile,
    // ensure that privileges cannot be regained. This can only be done if the
    // real user ID (and now the effective user ID as well) is not root, because
    // root always has permission to change identity.
    if (euid != uid) {
      CHECK_EQ(seteuid(euid), -1);
    }
    if (egid != gid) {
      CHECK_EQ(setegid(egid), -1);
    }
  }
#elif defined(OS_LINUX)
  PCHECK(setresgid(gid, gid, gid) == 0) << "setresgid";
  PCHECK(setresuid(uid, uid, uid) == 0) << "setresuid";

  // Don’t check to see if privileges can be regained on Linux, because on
  // Linux, it’s not as simple as ensuring that this can’t be done if non-root.
  // Instead, the ability to change user and group IDs are controlled by the
  // CAP_SETUID and CAP_SETGID capabilities, which may be granted to non-root
  // processes. Since the setresXid() interface is well-defined, it shouldn’t be
  // necessary to perform any additional checking anyway.
#else
#error Port this function to your system.
#endif
}

}  // namespace crashpad
