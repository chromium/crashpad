# Copyright 2017 The Crashpad Authors. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# This Makefile produces modules containing older formats of debug information.
#
# Build this with the tools from Visual C++ 6 (98), which was the last version
# to produce VC++2-type CodeView records (NB10) and embedded (as opposed to
# linked) CodeView debug info (NB11). (Older toolchains produced earlier
# versions of embedded CodeView, but it’s sufficient to generate any version for
# test data.)
#
# To build:
#
# nmake -nologo -f old_debug_modules.mak
#
# Given that this is quite tedious to build, the result is also checked in.

LINK=link

all: crashy_debug_link_nb10.dll \
     crashy_debug_embedded_nb11.dll \
     crashy_debug_link_dbg.dll


crashy_debug_link_nb10.obj: crashy_module.cc
	$(CC) /nologo /c /W4 /Zi /TP $** /Fo$@

# Contains an NB10 CodeView PDB link (the format introduced in Visual C++ 2.0)
# referencing crashy_debug_link_nb10.pdb.
crashy_debug_link_nb10.dll: crashy_debug_link_nb10.obj
	$(LINK) /nologo /dll /entry:DllMain /nodefaultlib /debug /debugtype:cv \
	        $** /out:$@


crashy_debug_embedded_nb11.obj: crashy_module.cc
	$(CC) /nologo /c /W4 /Z7 /TP $** /Fo$@

# Contains NB11-format CodeView data embedded in the DLL, along with an
# IMAGE_DEBUG_MISC carrying IMAGE_DEBUG_MISC_EXENAME referencing
# crashy_debug_embedded_nb11.dll.
crashy_debug_embedded_nb11.dll: crashy_debug_embedded_nb11.obj
	$(LINK) /nologo /dll /entry:DllMain /nodefaultlib /debug /debugtype:cv \
	        /pdb:none $** /out:$@


# Contains an IMAGE_DEBUG_MISC carrying IMAGE_DEBUG_MISC_EXENAME referencing
# crashy_debug_link_dbg.dbg, which is also produced. There is no CodeView
# record.
crashy_debug_link_dbg.dll: crashy_debug_embedded_nb11.dll
	del /f crashy_debug_link_dbg.dbg
	copy /y $** $@
	rebase -b 0x10000000 -x . $@


clean:
	del /f vc60.pdb \
	       crashy_debug_link_nb10.obj \
	       crashy_debug_link_nb10.dll \
	       crashy_debug_link_nb10.exp \
	       crashy_debug_link_nb10.ilk \
	       crashy_debug_link_nb10.lib \
	       crashy_debug_link_nb10.pdb \
	       crashy_debug_embedded_nb11.obj \
	       crashy_debug_embedded_nb11.dll \
	       crashy_debug_embedded_nb11.exp \
	       crashy_debug_embedded_nb11.ilk \
	       crashy_debug_embedded_nb11.lib \
	       crashy_debug_link_dbg.dll \
	       crashy_debug_link_dbg.dbg
