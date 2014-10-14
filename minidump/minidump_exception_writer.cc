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

#include "minidump/minidump_exception_writer.h"

#include "base/logging.h"

namespace crashpad {

MinidumpExceptionWriter::MinidumpExceptionWriter()
    : MinidumpStreamWriter(), exception_(), context_(nullptr) {
}

void MinidumpExceptionWriter::SetContext(MinidumpContextWriter* context) {
  DCHECK_EQ(state(), kStateMutable);

  context_ = context;
}

void MinidumpExceptionWriter::SetExceptionInformation(
    const std::vector<uint64_t>& exception_information) {
  DCHECK_EQ(state(), kStateMutable);

  const size_t parameters = exception_information.size();
  const size_t kMaxParameters =
      arraysize(exception_.ExceptionRecord.ExceptionInformation);
  CHECK_LE(parameters, kMaxParameters);

  exception_.ExceptionRecord.NumberParameters = parameters;
  size_t parameter = 0;
  for (; parameter < parameters; ++parameter) {
    exception_.ExceptionRecord.ExceptionInformation[parameter] =
        exception_information[parameter];
  }
  for (; parameter < kMaxParameters; ++parameter) {
    exception_.ExceptionRecord.ExceptionInformation[parameter] = 0;
  }
}

bool MinidumpExceptionWriter::Freeze() {
  DCHECK_EQ(state(), kStateMutable);
  CHECK(context_);

  if (!MinidumpStreamWriter::Freeze()) {
    return false;
  }

  context_->RegisterLocationDescriptor(&exception_.ThreadContext);

  return true;
}

size_t MinidumpExceptionWriter::SizeOfObject() {
  DCHECK_GE(state(), kStateFrozen);

  return sizeof(exception_);
}

std::vector<internal::MinidumpWritable*> MinidumpExceptionWriter::Children() {
  DCHECK_GE(state(), kStateFrozen);
  DCHECK(context_);

  std::vector<MinidumpWritable*> children;
  children.push_back(context_);

  return children;
}

bool MinidumpExceptionWriter::WriteObject(FileWriterInterface* file_writer) {
  DCHECK_EQ(state(), kStateWritable);

  return file_writer->Write(&exception_, sizeof(exception_));
}

MinidumpStreamType MinidumpExceptionWriter::StreamType() const {
  return kMinidumpStreamTypeException;
}

}  // namespace crashpad
