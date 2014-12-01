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

#include "util/mach/mach_message_util.h"

namespace crashpad {

void PrepareMIGReplyFromRequest(const mach_msg_header_t* in_header,
                                mach_msg_header_t* out_header) {
  out_header->msgh_bits =
      MACH_MSGH_BITS(MACH_MSGH_BITS_REMOTE(in_header->msgh_bits), 0);
  out_header->msgh_remote_port = in_header->msgh_remote_port;
  out_header->msgh_size = sizeof(mig_reply_error_t);
  out_header->msgh_local_port = MACH_PORT_NULL;
  out_header->msgh_id = in_header->msgh_id + 100;
  reinterpret_cast<mig_reply_error_t*>(out_header)->NDR = NDR_record;

  // MIG-generated dispatch routines don’t do this, but they should.
  out_header->msgh_reserved = 0;
}

void SetMIGReplyError(mach_msg_header_t* out_header, kern_return_t error) {
  reinterpret_cast<mig_reply_error_t*>(out_header)->RetCode = error;
}

const mach_msg_trailer_t* MachMessageTrailerFromHeader(
    const mach_msg_header_t* header) {
  vm_address_t header_address = reinterpret_cast<vm_address_t>(header);
  vm_address_t trailer_address = header_address + round_msg(header->msgh_size);
  return reinterpret_cast<const mach_msg_trailer_t*>(trailer_address);
}

}  // namespace crashpad
