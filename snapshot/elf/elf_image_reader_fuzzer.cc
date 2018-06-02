#include <inttypes.h>

#include "base/logging.h"
#include "snapshot/elf/elf_image_reader.h"
#include "util/process/process_memory.h"

using namespace crashpad;

class FakeProcessMemory : public ProcessMemory {
 public:
  FakeProcessMemory(const uint8_t* data, size_t size, VMAddress fake_base)
      : data_(data), size_(size), fake_base_(fake_base) {}

  ssize_t ReadUpTo(VMAddress address,
                   size_t size,
                   void* buffer) const override {
    VMAddress offset_in_data = address - fake_base_;
    ssize_t read_size = std::min(size_ - offset_in_data, size);
    memcpy(buffer, &data_[offset_in_data], read_size);
    return read_size;
  }

 private:
  const uint8_t* data_;
  size_t size_;
  VMAddress fake_base_;
};

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  // Swallow all logs to avoid spam.
  logging::SetLogMessageHandler(
      [](logging::LogSeverity, const char*, int, size_t, const std::string&) {
        return true;
      });

  constexpr size_t kBase = 0x10000;
  FakeProcessMemory process_memory(data, size, kBase);
  ProcessMemoryRange process_memory_range;
  process_memory_range.Initialize(&process_memory, true, kBase, size);

  ElfImageReader reader;
  if (!reader.Initialize(process_memory_range, kBase))
    return 0;

  ElfImageReader::NoteReader::Result result;
  std::string note_name;
  std::string note_desc;
  ElfImageReader::NoteReader::NoteType note_type;
  auto notes = reader.Notes(-1);
  while ((result = notes->NextNote(&note_name, &note_type, &note_desc)) ==
         ElfImageReader::NoteReader::Result::kSuccess) {
    LOG(ERROR) << note_name << note_type << note_desc;
  }

  return 0;
}
