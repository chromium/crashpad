/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
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

#include "third_party/apple_cctools/cctools/include/mach-o/getsect.h"

#if !defined(MAC_OS_X_VERSION_10_7) || \
    MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_7

#include <string.h>

#ifndef __LP64__
typedef struct mach_header mach_header_32_64;
typedef struct segment_command segment_command_32_64;
typedef struct section section_32_64;
#define LC_SEGMENT_32_64 LC_SEGMENT
#else /* defined(__LP64__) */
typedef struct mach_header_64 mach_header_32_64;
typedef struct segment_command_64 segment_command_32_64;
typedef struct section_64 section_32_64;
#define LC_SEGMENT_32_64 LC_SEGMENT_64
#endif /* defined(__LP64__) */

/*
 * This routine returns the a pointer to the section contents of the named
 * section in the named segment if it exists in the image pointed to by the
 * mach header.  Otherwise it returns zero.
 */

uint8_t *
crashpad_getsectiondata(
const mach_header_32_64 *mhp,
const char *segname,
const char *sectname,
unsigned long *size)
{
    segment_command_32_64 *sgp, *zero;
    section_32_64 *sp, *find;
    uint32_t i, j;

	zero = 0;
	find = 0;
	sp = 0;
	sgp = (segment_command_32_64 *)
	      ((char *)mhp + sizeof(mach_header_32_64));
	for(i = 0; i < mhp->ncmds; i++){
	    if(sgp->cmd == LC_SEGMENT_32_64){
		if(zero == 0 && sgp->fileoff == 0 && sgp->nsects != 0){
		    zero = sgp;
		    if(find != 0)
			goto done;
		}
		if(find == 0 &&
		   strncmp(sgp->segname, segname, sizeof(sgp->segname)) == 0){
		    sp = (section_32_64 *)((char *)sgp +
			 sizeof(segment_command_32_64));
		    for(j = 0; j < sgp->nsects; j++){
			if(strncmp(sp->sectname, sectname,
			   sizeof(sp->sectname)) == 0 &&
			   strncmp(sp->segname, segname,
			   sizeof(sp->segname)) == 0){
			    find = sp;
			    if(zero != 0)
				goto done;
			}
			sp = (section_32_64 *)((char *)sp +
			     sizeof(section_32_64));
		    }
		}
	    }
	    sgp = (segment_command_32_64 *)((char *)sgp + sgp->cmdsize);
	}
	return(0);
done:
	*size = sp->size;
	return((uint8_t *)((uintptr_t)mhp - zero->vmaddr + sp->addr));
}

uint8_t *
crashpad_getsegmentdata(
const mach_header_32_64 *mhp,
const char *segname,
unsigned long *size)
{
    segment_command_32_64 *sgp, *zero, *find;
    uint32_t i;

	zero = 0;
	find = 0;
	sgp = (segment_command_32_64 *)
	      ((char *)mhp + sizeof(mach_header_32_64));
	for(i = 0; i < mhp->ncmds; i++){
	    if(sgp->cmd == LC_SEGMENT_32_64){
		if(zero == 0 && sgp->fileoff == 0 && sgp->nsects != 0){
		    zero = sgp;
		    if(find != 0)
			goto done;
		}
		if(find == 0 &&
		   strncmp(sgp->segname, segname, sizeof(sgp->segname)) == 0){
		    find = sgp;
		    if(zero != 0)
			goto done;
		}
	    }
	    sgp = (segment_command_32_64 *)((char *)sgp + sgp->cmdsize);
	}
	return(0);
done:
	*size = sgp->vmsize;
	return((uint8_t *)((uintptr_t)mhp - zero->vmaddr + sgp->vmaddr));
}

#endif /* MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_7 */
