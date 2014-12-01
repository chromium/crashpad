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

#include "util/mach/mach_message_server.h"

#include <limits>

#include "base/mac/mach_logging.h"
#include "base/mac/scoped_mach_vm.h"
#include "util/misc/clock.h"

namespace crashpad {

namespace {

const int kNanosecondsPerMillisecond = 1E6;

// TimerRunning determines whether |deadline| has passed. If |deadline| is in
// the future, |*remaining_ms| is set to the number of milliseconds remaining,
// which will always be a positive value, and this function returns true. If
// |deadline| is zero (indicating that no timer is in effect), |*remaining_ms|
// is set to zero and this function returns true. Otherwise, this function
// returns false. |deadline| is specified on the same time base as is returned
// by ClockMonotonicNanoseconds().
bool TimerRunning(uint64_t deadline, mach_msg_timeout_t* remaining_ms) {
  if (!deadline) {
    *remaining_ms = MACH_MSG_TIMEOUT_NONE;
    return true;
  }

  uint64_t now = ClockMonotonicNanoseconds();

  if (now >= deadline) {
    return false;
  }

  uint64_t remaining = deadline - now;

  // Round to the nearest millisecond, taking care not to overflow.
  const int kHalfMillisecondInNanoseconds = kNanosecondsPerMillisecond / 2;
  mach_msg_timeout_t remaining_mach;
  if (remaining <=
      std::numeric_limits<uint64_t>::max() - kHalfMillisecondInNanoseconds) {
    remaining_mach = (remaining + kHalfMillisecondInNanoseconds) /
                     kNanosecondsPerMillisecond;
  } else {
    remaining_mach = remaining / kNanosecondsPerMillisecond;
  }

  if (remaining_mach == MACH_MSG_TIMEOUT_NONE) {
    return false;
  }

  *remaining_ms = remaining_mach;
  return true;
}

}  // namespace

// This implementation is based on 10.9.4
// xnu-2422.110.17/libsyscall/mach/mach_msg.c mach_msg_server_once(), but
// adapted to local style using scopers, replacing the server callback function
// and |max_size| parameter with a C++ interface, and with the addition of the
// the |persistent| parameter allowing this function to serve as a stand-in for
// mach_msg_server(), the |nonblocking| parameter to control blocking directly,
// and the |timeout_ms| parameter allowing this function to not block
// indefinitely.
//
// static
mach_msg_return_t MachMessageServer::Run(Interface* interface,
                                         mach_port_t receive_port,
                                         mach_msg_options_t options,
                                         Persistent persistent,
                                         Nonblocking nonblocking,
                                         ReceiveLarge receive_large,
                                         mach_msg_timeout_t timeout_ms) {
  options &= ~(MACH_RCV_MSG | MACH_SEND_MSG);

  mach_msg_options_t timeout_options = MACH_RCV_TIMEOUT | MACH_SEND_TIMEOUT |
                                       MACH_RCV_INTERRUPT | MACH_SEND_INTERRUPT;

  uint64_t deadline;
  if (nonblocking == kNonblocking) {
    options |= timeout_options;
    deadline = 0;
  } else if (timeout_ms != MACH_MSG_TIMEOUT_NONE) {
    options |= timeout_options;
    deadline = ClockMonotonicNanoseconds() +
               implicit_cast<uint64_t>(timeout_ms) * kNanosecondsPerMillisecond;
  } else {
    options &= ~timeout_options;
    deadline = 0;
  }

  if (receive_large == kReceiveLargeResize) {
    options |= MACH_RCV_LARGE;
  } else {
    options &= ~MACH_RCV_LARGE;
  }

  mach_msg_size_t trailer_alloc = REQUESTED_TRAILER_SIZE(options);
  mach_msg_size_t expected_request_size =
      interface->MachMessageServerRequestSize();
  mach_msg_size_t request_alloc =
      round_page(round_msg(expected_request_size) + trailer_alloc);
  mach_msg_size_t request_size =
      (receive_large == kReceiveLargeResize)
          ? request_alloc
          : round_msg(expected_request_size) + trailer_alloc;

  mach_msg_size_t max_reply_size = interface->MachMessageServerReplySize();

  // mach_msg_server() and mach_msg_server_once() would consider whether
  // |options| contains MACH_SEND_TRAILER and include MAX_TRAILER_SIZE in this
  // computation if it does, but that option is ineffective on OS X.
  mach_msg_size_t reply_alloc = round_page(max_reply_size);

  kern_return_t kr;

  do {
    mach_msg_size_t this_request_alloc = request_alloc;
    mach_msg_size_t this_request_size = request_size;

    base::mac::ScopedMachVM request_scoper;
    mach_msg_header_t* request_header = nullptr;

    while (!request_scoper.address()) {
      vm_address_t request_addr;
      kr = vm_allocate(mach_task_self(),
                       &request_addr,
                       this_request_alloc,
                       VM_FLAGS_ANYWHERE | VM_MAKE_TAG(VM_MEMORY_MACH_MSG));
      if (kr != KERN_SUCCESS) {
        return kr;
      }
      base::mac::ScopedMachVM trial_request_scoper(request_addr,
                                                   this_request_alloc);
      request_header = reinterpret_cast<mach_msg_header_t*>(request_addr);

      bool run_mach_msg_receive = false;
      do {
        // If |options| contains MACH_RCV_INTERRUPT, retry mach_msg() in a loop
        // when it returns MACH_RCV_INTERRUPTED to recompute |remaining_ms|
        // rather than allowing mach_msg() to retry using the original timeout
        // value. See 10.9.4 xnu-2422.110.17/libsyscall/mach/mach_msg.c
        // mach_msg().
        mach_msg_timeout_t remaining_ms;
        if (!TimerRunning(deadline, &remaining_ms)) {
          return MACH_RCV_TIMED_OUT;
        }

        kr = mach_msg(request_header,
                      options | MACH_RCV_MSG,
                      0,
                      this_request_size,
                      receive_port,
                      remaining_ms,
                      MACH_PORT_NULL);

        if (kr == MACH_RCV_TOO_LARGE && receive_large == kReceiveLargeIgnore) {
          MACH_LOG(WARNING, kr) << "mach_msg: ignoring large message";
          run_mach_msg_receive = true;
        } else if (kr == MACH_RCV_INTERRUPTED) {
          run_mach_msg_receive = true;
        }
      } while (run_mach_msg_receive);

      if (kr == MACH_MSG_SUCCESS) {
        request_scoper.swap(trial_request_scoper);
      } else if (kr == MACH_RCV_TOO_LARGE &&
                 receive_large == kReceiveLargeResize) {
        this_request_size =
            round_page(round_msg(request_header->msgh_size) + trailer_alloc);
        this_request_alloc = this_request_size;
      } else {
        return kr;
      }
    }

    vm_address_t reply_addr;
    kr = vm_allocate(mach_task_self(),
                     &reply_addr,
                     reply_alloc,
                     VM_FLAGS_ANYWHERE | VM_MAKE_TAG(VM_MEMORY_MACH_MSG));
    if (kr != KERN_SUCCESS) {
      return kr;
    }

    base::mac::ScopedMachVM reply_scoper(reply_addr, reply_alloc);

    mach_msg_header_t* reply_header =
        reinterpret_cast<mach_msg_header_t*>(reply_addr);
    bool destroy_complex_request = false;
    interface->MachMessageServerFunction(
        request_header, reply_header, &destroy_complex_request);

    if (!(reply_header->msgh_bits & MACH_MSGH_BITS_COMPLEX)) {
      // This only works if the reply message is not complex, because otherwise,
      // the location of the RetCode field is not known. It should be possible
      // to locate the RetCode field by looking beyond the descriptors in a
      // complex reply message, but this is not currently done. This behavior
      // has not proven itself necessary in practice, and it’s not done by
      // mach_msg_server() or mach_msg_server_once() either.
      mig_reply_error_t* reply_mig =
          reinterpret_cast<mig_reply_error_t*>(reply_header);
      if (reply_mig->RetCode == MIG_NO_REPLY) {
        reply_header->msgh_remote_port = MACH_PORT_NULL;
      } else if (reply_mig->RetCode != KERN_SUCCESS &&
                 request_header->msgh_bits & MACH_MSGH_BITS_COMPLEX) {
        destroy_complex_request = true;
      }
    }

    if (destroy_complex_request &&
        request_header->msgh_bits & MACH_MSGH_BITS_COMPLEX) {
      request_header->msgh_remote_port = MACH_PORT_NULL;
      mach_msg_destroy(request_header);
    }

    if (reply_header->msgh_remote_port != MACH_PORT_NULL) {
      // If the reply port right is a send-once right, the send won’t block even
      // if the remote side isn’t waiting for a message. No timeout is used,
      // which keeps the communication on the kernel’s fast path. If the reply
      // port right is a send right, MACH_SEND_TIMEOUT is used to avoid blocking
      // indefinitely. This duplicates the logic in 10.9.4
      // xnu-2422.110.17/libsyscall/mach/mach_msg.c mach_msg_server_once().
      mach_msg_option_t send_options =
          options | MACH_SEND_MSG |
          (MACH_MSGH_BITS_REMOTE(reply_header->msgh_bits) ==
                   MACH_MSG_TYPE_MOVE_SEND_ONCE
               ? 0
               : MACH_SEND_TIMEOUT);

      bool running;
      do {
        // If |options| contains MACH_SEND_INTERRUPT, retry mach_msg() in a loop
        // when it returns MACH_SEND_INTERRUPTED to recompute |remaining_ms|
        // rather than allowing mach_msg() to retry using the original timeout
        // value. See 10.9.4 xnu-2422.110.17/libsyscall/mach/mach_msg.c
        // mach_msg().
        mach_msg_timeout_t remaining_ms;
        running = TimerRunning(deadline, &remaining_ms);
        if (!running) {
          // Don’t return just yet. If the timer ran out in between the time the
          // request was received and now, at least try to send the response.
          remaining_ms = 0;
        }

        kr = mach_msg(reply_header,
                      send_options,
                      reply_header->msgh_size,
                      0,
                      MACH_PORT_NULL,
                      remaining_ms,
                      MACH_PORT_NULL);
      } while (kr == MACH_SEND_INTERRUPTED);

      if (kr != KERN_SUCCESS) {
        if (kr == MACH_SEND_INVALID_DEST || kr == MACH_SEND_TIMED_OUT) {
          mach_msg_destroy(reply_header);
        }
        return kr;
      }

      if (!running) {
        // The reply message was sent successfuly, so act as though the deadline
        // was reached before or during the subsequent receive operation when in
        // persistent mode, and just return success when not in persistent mode.
        return (persistent == kPersistent) ? MACH_RCV_TIMED_OUT : kr;
      }
    }
  } while (persistent == kPersistent);

  return kr;
}

}  // namespace crashpad
