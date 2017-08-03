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

//! \brief Reads x86-family CPU information by calling `cpuid`.
class CpuidReader {
 public:
  CpuidReader();
  ~CpuidReader();

  uint32_t Revision() const;

  std::string Vendor() const { return vendor_; }

  uint32_t Signature() const { return signature_; }

  uint64_t Features() const { return features_; }

  uint64_t ExtendedFeatures() const;

  uint32_t Leaf7Features() const;

  bool NXEnabled() const { return ExtendedFeatures() & (1 << 20); }

 private:
  void Cpuid(uint32_t cpuinfo[4], uint32_t leaf) const;

  std::string vendor_;
  uint32_t max_leaf_;
  uint32_t signature_;
  uint64_t features_;
};

}  // namespace internal
}  // namespace crashpad
