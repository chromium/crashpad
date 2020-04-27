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

#import <Foundation/Foundation.h>

#include <vector>

#include "gtest/gtest.h"
#include "testing/platform_test.h"

#include "base/logging.h"
#include "base/mac/mach_logging.h"
#include "base/macros.h"
#include "util/mach/exception_ports.h"
#include "util/mach/mach_extensions.h"
#include "util/mach/symbolic_constants_mach.h"

namespace crashpad {
namespace test {
namespace {

//! \brief Manages a pool of Mach send rights, deallocating all send rights upon
//!     destruction.
//!
//! This class effectively implements what a vector of
//! base::mac::ScopedMachSendRight objects would be.
//!
//! The various “show” operations performed by this program display Mach ports
//! by their names as they are known in this task. For this to be useful, rights
//! to the same ports must have consistent names across successive calls. This
//! cannot be guaranteed if the rights are deallocated as soon as they are used,
//! because if that deallocation causes the task to lose its last right to a
//! port, subsequently regaining a right to the same port would cause it to be
//! known by a new name in this task.
//!
//! Instead of immediately deallocating send rights that are used for display,
//! they can be added to this pool. The pool collects send rights, ensuring that
//! they remain alive in this task, and that subsequent calls that obtain the
//! same rights cause them to be known by the same name. All rights are
//! deallocated upon destruction.
class MachSendRightPool {
 public:
  MachSendRightPool() : send_rights_() {}

  ~MachSendRightPool() {
    for (mach_port_t send_right : send_rights_) {
      kern_return_t kr = mach_port_deallocate(mach_task_self(), send_right);
      MACH_LOG_IF(ERROR, kr != KERN_SUCCESS, kr) << "mach_port_deallocate";
    }
  }

  //! \brief Adds a send right to the pool.
  //!
  //! \param[in] send_right The send right to be added. The pool object takes
  //!     its own reference to the send right, which remains valid until the
  //!     pool object is destroyed. The caller remains responsible for its
  //!     reference to the send right.
  //!
  //! It is possible and in fact likely that one pool will wind up owning the
  //! same send right multiple times. This is acceptable, because send rights
  //! are reference-counted.
  void AddSendRight(mach_port_t send_right) {
    kern_return_t kr = mach_port_mod_refs(
        mach_task_self(), send_right, MACH_PORT_RIGHT_SEND, 1);
    MACH_CHECK(kr == KERN_SUCCESS, kr) << "mach_port_mod_refs";

    send_rights_.push_back(send_right);
  }

 private:
  std::vector<mach_port_t> send_rights_;

  DISALLOW_COPY_AND_ASSIGN(MachSendRightPool);
};

// Prints information about all exception ports known for |exception_ports|. If
// |numeric| is true, all information is printed in numeric form, otherwise, it
// will be converted to symbolic constants where possible by
// SymbolicConstantsMach. If |is_new| is true, information will be presented as
// “new exception ports”, indicating that they show the state of the exception
// ports after SetExceptionPort() has been called. Any send rights obtained by
// this function are added to |mach_send_right_pool|.
void ShowExceptionPorts(const ExceptionPorts& exception_ports,
                        bool numeric,
                        bool is_new,
                        MachSendRightPool* mach_send_right_pool) {
  const char* target_name = exception_ports.TargetTypeName();

  ExceptionPorts::ExceptionHandlerVector handlers;
  if (!exception_ports.GetExceptionPorts(ExcMaskValid(), &handlers)) {
    return;
  }

  const char* age_name = is_new ? "new " : "";

  if (handlers.empty()) {
    printf("no %s%s exception ports\n", age_name, target_name);
  }

  for (size_t port_index = 0; port_index < handlers.size(); ++port_index) {
    mach_send_right_pool->AddSendRight(handlers[port_index].port);

    if (numeric) {
      printf("%s%s exception port %zu, mask %#x, port %#x, "
             "behavior %#x, flavor %u\n",
             age_name,
             target_name,
             port_index,
             handlers[port_index].mask,
             handlers[port_index].port,
             handlers[port_index].behavior,
             handlers[port_index].flavor);
    } else {
      std::string mask_string = ExceptionMaskToString(
          handlers[port_index].mask, kUseShortName | kUnknownIsEmpty | kUseOr);
      if (mask_string.empty()) {
        mask_string.assign("?");
      }

      std::string behavior_string = ExceptionBehaviorToString(
          handlers[port_index].behavior, kUseShortName | kUnknownIsEmpty);
      if (behavior_string.empty()) {
        behavior_string.assign("?");
      }

      std::string flavor_string = ThreadStateFlavorToString(
          handlers[port_index].flavor, kUseShortName | kUnknownIsEmpty);
      if (flavor_string.empty()) {
        flavor_string.assign("?");
      }

      printf("%s%s exception port %zu, mask %#x (%s), port %#x, "
             "behavior %#x (%s), flavor %u (%s)\n",
             age_name,
             target_name,
             port_index,
             handlers[port_index].mask,
             mask_string.c_str(),
             handlers[port_index].port,
             handlers[port_index].behavior,
             behavior_string.c_str(),
             handlers[port_index].flavor,
             flavor_string.c_str());
    }
  }
}

using CrashpadIOSClient = PlatformTest;

TEST_F(CrashpadIOSClient, DumpWithoutCrash) {
  CrashpadClient client;
  client.StartCrashpadInProcessHandler();

  NativeCPUContext context;
#if defined(ARCH_CPU_X86_64)
  CaptureContext(&context);
#elif defined(ARCH_CPU_ARM64)
  // TODO(justincohen): Implement CaptureContext for ARM64.
  mach_msg_type_number_t thread_state_count = MACHINE_THREAD_STATE_COUNT;
  kern_return_t kr =
      thread_get_state(mach_thread_self(),
                       MACHINE_THREAD_STATE,
                       reinterpret_cast<thread_state_t>(&context),
                       &thread_state_count);
  ASSERT_EQ(kr, KERN_SUCCESS);
#endif
  client.DumpWithoutCrash(&context);
}

TEST_F(CrashpadIOSClient, ShowExceptionPorts) {
  MachSendRightPool mach_send_right_pool;
  ShowExceptionPorts(ExceptionPorts(ExceptionPorts::kTargetTypeTask, TASK_NULL),
                     false,
                     false,
                     &mach_send_right_pool);
}

// This test is covered by a similar XCUITest, but for development purposes
// it's sometimes easier and faster to run as a gtest.  However, there's no
// way to correctly run this as a gtest. Leave the test here, disabled, for use
// during development only.
TEST_F(CrashpadIOSClient, DISABLED_ThrowNSException) {
  CrashpadClient client;
  client.StartCrashpadInProcessHandler();
  [NSException raise:@"GtestNSException" format:@"ThrowException"];
}

// This test is covered by a similar XCUITest, but for development purposes
// it's sometimes easier and faster to run as a gtest.  However, there's no
// way to correctly run this as a gtest. Leave the test here, disabled, for use
// during development only.
TEST_F(CrashpadIOSClient, DISABLED_ThrowException) {
  CrashpadClient client;
  client.StartCrashpadInProcessHandler();
  std::vector<int> empty_vector;
  empty_vector.at(42);
}

}  // namespace
}  // namespace test
}  // namespace crashpad
