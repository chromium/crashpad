list(APPEND ZLIB_SOURCES
    zlib/adler32.c
    zlib/compress.c
    zlib/crc32.c
    zlib/crc32.h
    zlib/deflate.c
    zlib/deflate.h
    zlib/gzclose.c
    zlib/gzguts.h
    zlib/gzlib.c
    zlib/gzread.c
    zlib/gzwrite.c
    zlib/infback.c
    zlib/inffast.c
    zlib/inffast.h
    zlib/inffixed.h
    zlib/inflate.c
    zlib/inflate.h
    zlib/inftrees.c
    zlib/inftrees.h
    zlib/names.h
    zlib/trees.c
    zlib/trees.h
    zlib/uncompr.c
    zlib/zconf.h
    zlib/zlib.h
    zlib/zutil.c
    zlib/zutil.h
    zlib_crashpad.h
)

# TODO: x86/x64
list(APPEND ZLIB_SOURCES
    zlib/crc_folding.c
    zlib/fill_window_sse.c
    zlib/x86.c
    zlib/x86.h
)

add_library(zlib STATIC ${ZLIB_SOURCES})
target_compile_definitions(zlib PUBLIC HAVE_STDARG_H)

include_directories(zlib)

if(MSVC)
    target_compile_options(zlib PUBLIC
        "/wd4131" # uses old-style declarator
        "/wd4244" # conversion from 't1' to 't2', possible loss of data
        "/wd4245" # conversion from 't1' to 't2', signed/unsigned mismatch
        "/wd4267" # conversion from 'size_t' to 't', possible loss of data
        "/wd4324" # structure was padded due to alignment specifier
        "/wd4702" # unreachable code
    )
endif()
