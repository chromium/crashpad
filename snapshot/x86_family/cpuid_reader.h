// Copyright 2017 The Crashpad Authors. All rights reserved.
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

#include <stdint.h>

#include <string>

namespace crashpad {
namespace internal {

class CpuidReader {
 public:
  CpuidReader() : vendor_() {
    uint32_t cpuinfo[4];
    Cpuid(cpuinfo, 0);
    max_leaf_ = cpuinfo[0];
    vendor_.append(reinterpret_cast<char*>(&cpuinfo[1]), 4);
    vendor_.append(reinterpret_cast<char*>(&cpuinfo[3]), 4);
    vendor_.append(reinterpret_cast<char*>(&cpuinfo[2]), 4);

    Cpuid(cpuinfo, 1);
    signature_ = cpuinfo[0];
    features_ = (static_cast<uint64_t>(cpuinfo[2]) << 32) |
                static_cast<uint64_t>(cpuinfo[3]);
  }
  ~CpuidReader() {}

  uint32_t Revision() const {
    uint8_t stepping = signature_ & 0xf;
    uint8_t model = (signature_ & 0xf0) >> 4;
    uint8_t family = (signature_ & 0xf00) >> 8;
    uint8_t extended_model = static_cast<uint8_t>((signature_ & 0xf0000) >> 16);
    uint16_t extended_family = (signature_ & 0xff00000) >> 20;

    // For families before 15, extended_family are simply reserved bits.
    if (family < 15)
      extended_family = 0;
    // extended_model is only used for families 6 and 15.
    if (family != 6 && family != 15)
      extended_model = 0;

    uint16_t adjusted_family = family + extended_family;
    uint8_t adjusted_model = model + (extended_model << 4);
    return (adjusted_family << 16) | (adjusted_model << 8) | stepping;
  }

  std::string Vendor() const { return vendor_; }

  uint32_t Signature() const { return signature_; }

  uint64_t Features() const { return features_; }

  uint64_t ExtendedFeatures() const {
    uint32_t cpuinfo[4];
    Cpuid(cpuinfo, 0x80000001);
    return (static_cast<uint64_t>(cpuinfo[2]) << 32) |
           static_cast<uint64_t>(cpuinfo[3]);
  }

  uint32_t Leaf7Features() const {
    if (max_leaf_ < 7) {
      return 0;
    }
    uint32_t cpuinfo[4];
    Cpuid(cpuinfo, 7);
    return cpuinfo[1];
  }

  bool NXEnabled() const { return ExtendedFeatures() & (1 << 20); }

 private:
  void Cpuid(uint32_t cpuinfo[4], uint32_t leaf) const {
    asm("cpuid"
        : "=a"(cpuinfo[0]), "=b"(cpuinfo[1]), "=c"(cpuinfo[2]), "=d"(cpuinfo[3])
        : "a"(leaf), "b"(0), "c"(0), "d"(0));
  }

  std::string vendor_;
  uint32_t max_leaf_;
  uint32_t signature_;
  uint64_t features_;
};

}  // namespace internal
}  // namespace crashpad
