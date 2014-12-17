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

#include "util/mach/mach_extensions.h"

#include "base/mac/scoped_mach_port.h"
#include "gtest/gtest.h"
#include "util/test/mac/mach_errors.h"

namespace crashpad {
namespace test {
namespace {

TEST(MachExtensions, MachThreadSelf) {
  base::mac::ScopedMachSendRight thread_self(mach_thread_self());
  EXPECT_EQ(thread_self, MachThreadSelf());
}

TEST(MachExtensions, NewMachPort_Receive) {
  base::mac::ScopedMachReceiveRight port(NewMachPort(MACH_PORT_RIGHT_RECEIVE));
  ASSERT_NE(kMachPortNull, port);

  mach_port_type_t type;
  kern_return_t kr = mach_port_type(mach_task_self(), port, &type);
  ASSERT_EQ(KERN_SUCCESS, kr) << MachErrorMessage(kr, "mach_port_get_type");

  EXPECT_EQ(MACH_PORT_TYPE_RECEIVE, type);
}

TEST(MachExtensions, NewMachPort_PortSet) {
  base::mac::ScopedMachPortSet port(NewMachPort(MACH_PORT_RIGHT_PORT_SET));
  ASSERT_NE(kMachPortNull, port);

  mach_port_type_t type;
  kern_return_t kr = mach_port_type(mach_task_self(), port, &type);
  ASSERT_EQ(KERN_SUCCESS, kr) << MachErrorMessage(kr, "mach_port_get_type");

  EXPECT_EQ(MACH_PORT_TYPE_PORT_SET, type);
}

TEST(MachExtensions, NewMachPort_DeadName) {
  base::mac::ScopedMachSendRight port(NewMachPort(MACH_PORT_RIGHT_DEAD_NAME));
  ASSERT_NE(kMachPortNull, port);

  mach_port_type_t type;
  kern_return_t kr = mach_port_type(mach_task_self(), port, &type);
  ASSERT_EQ(KERN_SUCCESS, kr) << MachErrorMessage(kr, "mach_port_get_type");

  EXPECT_EQ(MACH_PORT_TYPE_DEAD_NAME, type);
}

}  // namespace
}  // namespace test
}  // namespace crashpad
