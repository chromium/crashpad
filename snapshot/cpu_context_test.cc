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

#include "snapshot/cpu_context.h"

#include <string.h>
#include <sys/types.h>

#include "base/macros.h"
#include "gtest/gtest.h"

namespace crashpad {
namespace test {
namespace {

enum ExponentValue {
  kExponentAllZero = 0,
  kExponentAllOne,
  kExponentNormal,
};

enum FractionValue {
  kFractionAllZero = 0,
  kFractionNormal,
};

//! \brief Initializes an x87 register to a known bit pattern.
//!
//! \param[out] st_mm The x87 register to initialize. The reserved portion of
//!     the register is always zeroed out.
//! \param[in] exponent_value The bit pattern to use for the exponent. If this
//!     is kExponentAllZero, the sign bit will be set to `1`, and if this is
//!     kExponentAllOne, the sign bit will be set to `0`. This tests that the
//!     implementation doesn’t erroneously consider the sign bit to be part of
//!     the exponent. This may also be kExponentNormal, indicating that the
//!     exponent shall neither be all zeroes nor all ones.
//! \param[in] j_bit The value to use for the “J bit” (“integer bit”).
//! \param[in] fraction_value If kFractionAllZero, the fraction will be zeroed
//!     out. If kFractionNormal, the fraction will not be all zeroes.
void SetX87Register(CPUContextX86::X87OrMMXRegister* st_mm,
                    ExponentValue exponent_value,
                    bool j_bit,
                    FractionValue fraction_value) {
  switch (exponent_value) {
    case kExponentAllZero:
      st_mm->st[9] = 0x80;
      st_mm->st[8] = 0;
      break;
    case kExponentAllOne:
      st_mm->st[9] = 0x7f;
      st_mm->st[8] = 0xff;
      break;
    case kExponentNormal:
      st_mm->st[9] = 0x55;
      st_mm->st[8] = 0x55;
      break;
  }

  uint8_t fraction_pattern = fraction_value == kFractionAllZero ? 0 : 0x55;
  memset(&st_mm->st[0], fraction_pattern, 8);

  if (j_bit) {
    st_mm->st[7] |= 0x80;
  } else {
    st_mm->st[7] &= ~0x80;
  }

  memset(st_mm->st_reserved, 0, sizeof(st_mm->st_reserved));
}

TEST(CPUContextX86, FxsaveToFsaveTagWord) {
  // The fsave tag word uses bit pattern 00 for valid, 01 for zero, 10 for
  // “special”, and 11 for empty. Like the fxsave tag word, it is arranged by
  // physical register. The fxsave tag word determines whether a register is
  // empty, and analysis of the x87 register content distinguishes between
  // valid, zero, and special. In the initializations below, comments show
  // whether a register is expected to be considered valid, zero, or special,
  // except where the tag word is expected to indicate that it is empty. Each
  // combination appears twice: once where the fxsave tag word indicates a
  // nonempty register, and once again where it indicates an empty register.

  uint16_t fsw = 0 << 11;  // top = 0: logical 0-7 maps to physical 0-7
  uint8_t fxsave_tag = 0x0f;  // physical 4-7 (logical 4-7) empty
  CPUContextX86::X87OrMMXRegister st_mm[8];
  SetX87Register(&st_mm[0], kExponentNormal, false, kFractionNormal);  // spec.
  SetX87Register(&st_mm[1], kExponentNormal, true, kFractionNormal);  // valid
  SetX87Register(&st_mm[2], kExponentNormal, false, kFractionAllZero);  // spec.
  SetX87Register(&st_mm[3], kExponentNormal, true, kFractionAllZero);  // valid
  SetX87Register(&st_mm[4], kExponentNormal, false, kFractionNormal);
  SetX87Register(&st_mm[5], kExponentNormal, true, kFractionNormal);
  SetX87Register(&st_mm[6], kExponentNormal, false, kFractionAllZero);
  SetX87Register(&st_mm[7], kExponentNormal, true, kFractionAllZero);
  EXPECT_EQ(0xff22,
            CPUContextX86::FxsaveToFsaveTagWord(fsw, fxsave_tag, st_mm));

  fsw = 2 << 11;  // top = 2: logical 0-7 maps to physical 2-7, 0-1
  fxsave_tag = 0xf0;  // physical 0-3 (logical 6-7, 0-1) empty
  SetX87Register(&st_mm[0], kExponentAllZero, false, kFractionNormal);
  SetX87Register(&st_mm[1], kExponentAllZero, true, kFractionNormal);
  SetX87Register(&st_mm[2], kExponentAllZero, false, kFractionAllZero);  // zero
  SetX87Register(&st_mm[3], kExponentAllZero, true, kFractionAllZero);  // spec.
  SetX87Register(&st_mm[4], kExponentAllZero, false, kFractionNormal);  // spec.
  SetX87Register(&st_mm[5], kExponentAllZero, true, kFractionNormal);  // spec.
  SetX87Register(&st_mm[6], kExponentAllZero, false, kFractionAllZero);
  SetX87Register(&st_mm[7], kExponentAllZero, true, kFractionAllZero);
  EXPECT_EQ(0xa9ff,
            CPUContextX86::FxsaveToFsaveTagWord(fsw, fxsave_tag, st_mm));

  fsw = 5 << 11;  // top = 5: logical 0-7 maps to physical 5-7, 0-4
  fxsave_tag = 0x5a;  // physical 0, 2, 5, and 7 (logical 5, 0, 2, and 3) empty
  SetX87Register(&st_mm[0], kExponentAllOne, false, kFractionNormal);
  SetX87Register(&st_mm[1], kExponentAllOne, true, kFractionNormal);  // spec.
  SetX87Register(&st_mm[2], kExponentAllOne, false, kFractionAllZero);
  SetX87Register(&st_mm[3], kExponentAllOne, true, kFractionAllZero);
  SetX87Register(&st_mm[4], kExponentAllOne, false, kFractionNormal);  // spec.
  SetX87Register(&st_mm[5], kExponentAllOne, true, kFractionNormal);
  SetX87Register(&st_mm[6], kExponentAllOne, false, kFractionAllZero);  // spec.
  SetX87Register(&st_mm[7], kExponentAllOne, true, kFractionAllZero);  // spec.
  EXPECT_EQ(0xeebb,
            CPUContextX86::FxsaveToFsaveTagWord(fsw, fxsave_tag, st_mm));

  // This set set is just a mix of all of the possible tag types in a single
  // register file.
  fsw = 1 << 11;  // top = 1: logical 0-7 maps to physical 1-7, 0
  fxsave_tag = 0x1f;  // physical 5-7 (logical 4-6) empty
  SetX87Register(&st_mm[0], kExponentNormal, true, kFractionAllZero);  // valid
  SetX87Register(&st_mm[1], kExponentAllZero, false, kFractionAllZero);  // zero
  SetX87Register(&st_mm[2], kExponentAllOne, true, kFractionAllZero);  // spec.
  SetX87Register(&st_mm[3], kExponentAllOne, true, kFractionNormal);  // spec.
  SetX87Register(&st_mm[4], kExponentAllZero, false, kFractionAllZero);
  SetX87Register(&st_mm[5], kExponentAllZero, false, kFractionAllZero);
  SetX87Register(&st_mm[6], kExponentAllZero, false, kFractionAllZero);
  SetX87Register(&st_mm[7], kExponentNormal, true, kFractionNormal);  // valid
  EXPECT_EQ(0xfe90,
            CPUContextX86::FxsaveToFsaveTagWord(fsw, fxsave_tag, st_mm));

  // In this set, everything is valid.
  fsw = 0 << 11;  // top = 0: logical 0-7 maps to physical 0-7
  fxsave_tag = 0xff;  // nothing empty
  for (size_t index = 0; index < arraysize(st_mm); ++index) {
    SetX87Register(&st_mm[index], kExponentNormal, true, kFractionAllZero);
  }
  EXPECT_EQ(0, CPUContextX86::FxsaveToFsaveTagWord(fsw, fxsave_tag, st_mm));

  // In this set, everything is empty. The registers shouldn’t be consulted at
  // all, so they’re left alone from the previous set.
  fsw = 0 << 11;  // top = 0: logical 0-7 maps to physical 0-7
  fxsave_tag = 0;  // everything empty
  EXPECT_EQ(0xffff,
            CPUContextX86::FxsaveToFsaveTagWord(fsw, fxsave_tag, st_mm));
}

}  // namespace
}  // namespace test
}  // namespace crashpad
