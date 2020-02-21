list(APPEND GETOPT_SOURCES
    getopt.cc
    getopt.h
)

add_library(getopt STATIC ${GETOPT_SOURCES})
