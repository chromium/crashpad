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

#ifndef CRASHPAD_UTIL_MACH_MACH_MESSAGE_UTIL_H_
#define CRASHPAD_UTIL_MACH_MACH_MESSAGE_UTIL_H_

#include <mach/mach.h>

namespace crashpad {

//! \brief Initializes a reply message for a MIG server routine based on its
//!     corresponding request.
//!
//! If a request is handled by a server routine, it may be necessary to revise
//! some of the fields set by this function, such as `msgh_size` and any fields
//! defined in a routine’s reply structure type.
//!
//! \param[in] in_header The request message to base the reply on.
//! \param[out] out_header The reply message to initialize. \a out_header will
//!     be treated as a `mig_reply_error_t*` and all of its fields will be set
//!     except for `RetCode`, which must be set by SetMIGReplyError(). This
//!     argument is accepted as a `mach_msg_header_t*` instead of a
//!     `mig_reply_error_t*` because that is the type that callers are expected
//!     to possess in the C API.
void PrepareMIGReplyFromRequest(const mach_msg_header_t* in_header,
                                mach_msg_header_t* out_header);

//! \brief Sets the error code in a reply message for a MIG server routine.
//!
//! \param[inout] out_header The reply message to operate on. \a out_header will
//!     be treated as a `mig_reply_error_t*` and its `RetCode` field will be
//!     set. This argument is accepted as a `mach_msg_header_t*` instead of a
//!     `mig_reply_error_t*` because that is the type that callers are expected
//!     to possess in the C API.
//! \param[in] error The error code to store in \a out_header.
//!
//! \sa PrepareMIGReplyFromRequest()
void SetMIGReplyError(mach_msg_header_t* out_header, kern_return_t error);

//! \brief Returns a Mach message trailer for a message that has been received.
//!
//! This function must only be called on Mach messages that have been received
//! via the Mach messaging interface, such as `mach_msg()`. Messages constructed
//! for sending do not contain trailers.
//!
//! \param[in] header A pointer to a received Mach message.
//!
//! \return A pointer to the trailer following the received Mach message’s body.
//!     The contents of the trailer depend on the options provided to
//!     `mach_msg()` or a similar function when the message was received.
const mach_msg_trailer_t* MachMessageTrailerFromHeader(
    const mach_msg_header_t* header);

}  // namespace crashpad

#endif  // CRASHPAD_UTIL_MACH_MACH_MESSAGE_UTIL_H_
