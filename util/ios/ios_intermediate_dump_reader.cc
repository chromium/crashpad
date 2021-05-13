// Copyright 2021 The Crashpad Authors. All rights reserved.
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

#include "util/ios/ios_intermediate_dump_reader.h"

#include <memory>
#include <stack>

#include "base/logging.h"
#include "util/ios/ios_intermediate_dump_data.h"
#include "util/ios/ios_intermediate_dump_list.h"
#include "util/ios/ios_intermediate_dump_object.h"
#include "util/ios/ios_intermediate_dump_writer.h"

namespace crashpad {
namespace internal {

bool IOSIntermediateDumpReader::Initialize(const base::FilePath& path) {
  reader_ = std::make_unique<crashpad::FileReader>();
  if (!reader_->Open(path)) {
    return false;
  }

  // Don't initialize empty files.
  if (reader_->Seek(0, SEEK_END) == 0)
    return false;

  return true;
}

bool IOSIntermediateDumpReader::Parse() {
  if (!ParseInternal(reader_.get(), minidump_)) {
    DLOG(ERROR) << "Intermediate dump parsing failed";
    return false;
  }

  // Useful for seeing what keys have been parsed.
#if !defined(NDEBUG) && 0
  minidump_.DebugDump();
#endif
  return true;
}

bool IOSIntermediateDumpReader::ParseInternal(
    FileReaderInterface* reader,
    IOSIntermediateDumpMap& mainDocument) {
  if (!reader->SeekSet(0)) {
    return false;
  }
  std::stack<IOSIntermediateDumpObject*> parent;
  parent.push(&mainDocument);
  using Command = IOSIntermediateDumpWriter::CommandType;
  using Type = IOSIntermediateDumpObject::Type;

  Command command;
  while (reader->ReadExactly(&command, sizeof(Command))) {
    switch (command) {
      case Command::MAP_START: {
        std::unique_ptr<IOSIntermediateDumpMap> newDocument(
            new IOSIntermediateDumpMap());
        if (parent.top()->type() == Type::kMap) {
          auto parentMap = static_cast<IOSIntermediateDumpMap*>(parent.top());
          parent.push(newDocument.get());
          IntermediateDumpKey key;
          if (!reader->ReadExactly(&key, sizeof(key)))
            return false;
          if (key == IntermediateDumpKey::kInvalid)
            return false;
          parentMap->map_[key] = std::move(newDocument);
        } else if (parent.top()->type() == Type::kList) {
          auto parentList = static_cast<IOSIntermediateDumpList*>(parent.top());
          parent.push(newDocument.get());
          parentList->push_back(std::move(newDocument));
        } else {
          DLOG(ERROR) << "Unexpected parent document.";
          return false;
        }
      } break;
      case Command::ARRAY_START: {
        auto newList = std::make_unique<IOSIntermediateDumpList>();
        if (parent.top()->type() != Type::kMap) {
          DLOG(ERROR) << "Attempting to push an array not in a map.";
          return false;
        }

        IntermediateDumpKey key;
        if (!reader->ReadExactly(&key, sizeof(key)))
          return false;
        if (key == IntermediateDumpKey::kInvalid)
          return false;

        auto parentMap = static_cast<IOSIntermediateDumpMap*>(parent.top());
        parent.push(newList.get());
        parentMap->map_[key] = std::move(newList);
      } break;
      case Command::MAP_END:
      case Command::ARRAY_END:
        if (parent.size() < 2) {
          DLOG(ERROR) << "Attempting to pop off main document.";
          return false;
        }
        parent.pop();
        break;
      case Command::PROPERTY: {
        if (parent.top()->type() != Type::kMap) {
          DLOG(ERROR) << "Attempting to add a property not in a map.";
          return false;
        }
        IntermediateDumpKey key;
        if (!reader->ReadExactly(&key, sizeof(key)))
          return false;
        if (key == IntermediateDumpKey::kInvalid)
          return false;

        off_t value_length;
        if (!reader->ReadExactly(&value_length, sizeof(value_length))) {
          return false;
        }

        constexpr int kMaximumPropertyLength = 64 * 1024 * 1024;  // 64MB.
        if (value_length > kMaximumPropertyLength) {
          DLOG(ERROR) << "Attempting to read a property that's too big: "
                      << value_length;
          return false;
        }

        auto data = std::make_unique<uint8_t[]>(value_length);
        if (!reader->ReadExactly(data.get(), value_length)) {
          return false;
        }
        auto parentMap = static_cast<IOSIntermediateDumpMap*>(parent.top());
        parentMap->map_[key] = std::make_unique<IOSIntermediateDumpData>(
            std::move(data), value_length);
      } break;
      case Command::DOCUMENT_END: {
        if (parent.size() != 1) {
          DLOG(ERROR) << "Unexpected end of main document.";
          return false;
        }
        return true;
      }
      default:
        DLOG(ERROR) << "Failed to parse serialized interim minidump.";
        return false;
    }
  }

  DLOG(ERROR) << "Unexpected end of main document.";
  return false;
}

}  // namespace internal
}  // namespace crashpad
