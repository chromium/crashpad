
#include "minidump/minidump_stacktrace_writer.h"

#include <stddef.h>

#include <limits>
#include <utility>

#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "minidump/minidump_string_writer.h"
#include "minidump/minidump_writer_util.h"
#include "snapshot/thread_snapshot.h"
#include "util/file/file_writer.h"
#include "util/misc/implicit_cast.h"
#include "util/numeric/in_range_cast.h"
#include "util/numeric/safe_assignment.h"

namespace crashpad {

MinidumpStacktraceListWriter::MinidumpStacktraceListWriter()
    : MinidumpStreamWriter(),
      threads_(),
      frames_(),
      symbol_bytes_(),
      stacktrace_header_() {}

MinidumpStacktraceListWriter::~MinidumpStacktraceListWriter() {}

void MinidumpStacktraceListWriter::InitializeFromSnapshot(
    const std::vector<const ThreadSnapshot*>& thread_snapshots,
    const MinidumpThreadIDMap& thread_id_map) {
  DCHECK_EQ(state(), kStateMutable);

  DCHECK(threads_.empty());
  DCHECK(frames_.empty());
  DCHECK(symbol_bytes_.empty());

  for (auto thread_snapshot : thread_snapshots) {
    internal::RawThread thread;
    thread.thread_id = thread_snapshot->ThreadID();
    thread.start_frame = frames_.size();

    // TODO: Create a stub that will later return a real stack trace:
    // That would be https://getsentry.atlassian.net/browse/NATIVE-198
    // auto frames = thread_snapshot->StackTrace();
    std::vector<FrameSnapshot> frames;
    frames.emplace_back(0xfff70001, std::string("uiaeo"));
    frames.emplace_back(0xfff70002, std::string("snrtdy"));

    for (auto frame_snapshot : frames) {
      internal::RawFrame frame;
      frame.instruction_addr = frame_snapshot.InstructionAddr();
      frame.symbol_offset = symbol_bytes_.size();

      auto symbol = frame_snapshot.Symbol();

      symbol_bytes_.reserve(symbol.size());
      symbol_bytes_.insert(symbol_bytes_.end(), symbol.begin(), symbol.end());

      frame.symbol_len = symbol.size();

      frames_.push_back(frame);
    }

    thread.num_frames = frames_.size() - thread.start_frame;

    threads_.push_back(thread);
  }

  stacktrace_header_.version = 1;
  stacktrace_header_.num_threads = threads_.size();
  stacktrace_header_.num_frames = frames_.size();
  stacktrace_header_.symbol_bytes = symbol_bytes_.size();
}

size_t MinidumpStacktraceListWriter::SizeOfObject() {
  DCHECK_GE(state(), kStateFrozen);

  return sizeof(stacktrace_header_) +
         threads_.size() * sizeof(internal::RawThread) +
         frames_.size() * sizeof(internal::RawFrame) + symbol_bytes_.size();
}

size_t MinidumpStacktraceListWriter::Alignment() {
  // because we are writing `uint64_t` that are 8-byte aligned
  return 8;
}

bool MinidumpStacktraceListWriter::WriteObject(
    FileWriterInterface* file_writer) {
  DCHECK_EQ(state(), kStateWritable);

  WritableIoVec iov;
  // header, threads, frames, symbol_bytes
  std::vector<WritableIoVec> iovecs(4);

  iov.iov_base = &stacktrace_header_;
  iov.iov_len = sizeof(stacktrace_header_);
  iovecs.push_back(iov);

  iov.iov_base = &threads_.front();
  iov.iov_len = threads_.size() * sizeof(internal::RawThread);
  iovecs.push_back(iov);

  iov.iov_base = &frames_.front();
  iov.iov_len = frames_.size() * sizeof(internal::RawFrame);
  iovecs.push_back(iov);

  iov.iov_base = &symbol_bytes_.front();
  iov.iov_len = symbol_bytes_.size();
  iovecs.push_back(iov);

  return file_writer->WriteIoVec(&iovecs);
}

MinidumpStreamType MinidumpStacktraceListWriter::StreamType() const {
  return kMinidumpStreamTypeSentryStackTraces;
}

}  // namespace crashpad
