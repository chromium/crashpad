list(APPEND TOOLS_SOURCES
    tool_support.cc
    tool_support.h
)

add_library(crashpad_tools STATIC ${TOOLS_SOURCES})
target_link_libraries(crashpad_tools
    mini_chromium
)

# TODO: do we need any tool executables?
