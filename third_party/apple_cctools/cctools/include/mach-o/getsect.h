/*
 * Copyright (c) 2004 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
#ifndef CRASHPAD_THIRD_PARTY_APPLE_CCTOOLS_CCTOOLS_INCLUDE_MACH_O_GETSECT_H_
#define CRASHPAD_THIRD_PARTY_APPLE_CCTOOLS_CCTOOLS_INCLUDE_MACH_O_GETSECT_H_

#include <AvailabilityMacros.h>

#if !defined(MAC_OS_X_VERSION_10_7) || \
    MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_7

#include <stdint.h>
#include <mach-o/loader.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifndef __LP64__
/*
 * Runtime interfaces for 32-bit Mach-O programs.
 */
extern uint8_t *crashpad_getsectiondata(
    const struct mach_header *mhp,
    const char *segname,
    const char *sectname,
    unsigned long *size);

extern uint8_t *crashpad_getsegmentdata(
    const struct mach_header *mhp,
    const char *segname,
    unsigned long *size);

#else /* defined(__LP64__) */
/*
 * Runtime interfaces for 64-bit Mach-O programs.
 */
extern uint8_t *crashpad_getsectiondata(
    const struct mach_header_64 *mhp,
    const char *segname,
    const char *sectname,
    unsigned long *size);

extern uint8_t *crashpad_getsegmentdata(
    const struct mach_header_64 *mhp,
    const char *segname,
    unsigned long *size);

#endif /* defined(__LP64__) */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_7 */

#endif /* CRASHPAD_THIRD_PARTY_APPLE_CCTOOLS_CCTOOLS_INCLUDE_MACH_O_GETSECT_H_ */
