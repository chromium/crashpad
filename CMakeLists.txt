cmake_minimum_required(VERSION 3.10)
project(crashpad LANGUAGES C CXX)

set(CRASHPAD_MAIN_PROJECT OFF)
if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    set(CRASHPAD_MAIN_PROJECT ON)
endif()

option(CRASHPAD_ENABLE_INSTALL "Enable crashpad installation" "${CRASHPAD_MAIN_PROJECT}")
option(CRASHPAD_ENABLE_INSTALL_DEV "Enable crashpad development installation" "${CRASHPAD_MAIN_PROJECT}")

if(MSVC)
    set(CRASHPAD_ZLIB_SYSTEM_DEFAULT OFF)
else()
    set(CRASHPAD_ZLIB_SYSTEM_DEFAULT ON)
endif()
option(CRASHPAD_ZLIB_SYSTEM "Use system zlib library" "${CRASHPAD_ZLIB_SYSTEM_DEFAULT}")

if(CRASHPAD_ZLIB_SYSTEM)
    find_package(ZLIB REQUIRED)
endif()

include(GNUInstallDirs)
set(CMAKE_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/crashpad")

function(crashpad_install_target)
    if(CRASHPAD_ENABLE_INSTALL)
        install(TARGETS ${ARGN} EXPORT crashpad_export
            ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
            LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
            RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
        )
    endif()
endfunction()
function(crashpad_install_dev)
    if(CRASHPAD_ENABLE_INSTALL_DEV)
        install(${ARGN})
    endif()
endfunction()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(LINUX TRUE)
endif()

if(WIN32)
    enable_language(ASM_MASM)
    IF(MINGW)
        find_program(UASM uasm)
        if(UASM)
            SET(CMAKE_ASM_MASM_COMPILER "uasm")
            SET(CMAKE_ASM_MASM_FLAGS "-win64 -10")
        else()
            message(FATAL_ERROR "UASM is required for MinGW builds! Make sure you have it installed")
        endif()
    endif()
else()
    enable_language(ASM)
endif()

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(crashpad_interface INTERFACE)
target_include_directories(crashpad_interface INTERFACE
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/third_party/mini_chromium/mini_chromium>
)
target_compile_definitions(crashpad_interface INTERFACE
    CRASHPAD_LSS_SOURCE_EMBEDDED
)

if(MSVC)
    target_compile_definitions(crashpad_interface INTERFACE
        NOMINMAX
        UNICODE
        WIN32_LEAN_AND_MEAN
        _CRT_SECURE_NO_WARNINGS
        _HAS_EXCEPTIONS=0
        _UNICODE
    )
    target_compile_options(crashpad_interface INTERFACE
        $<$<COMPILE_LANGUAGE:C,CXX>:/FS>
        $<$<COMPILE_LANGUAGE:C,CXX>:/W4>
        $<$<COMPILE_LANGUAGE:C,CXX>:/WX>
        $<$<COMPILE_LANGUAGE:C,CXX>:/Zi>
        $<$<COMPILE_LANGUAGE:C,CXX>:/bigobj> # Support larger number of sections in obj file.
        $<$<COMPILE_LANGUAGE:C,CXX>:/wd4100> # Unreferenced formal parameter.
        $<$<COMPILE_LANGUAGE:C,CXX>:/wd4127> # Conditional expression is constant.
        $<$<COMPILE_LANGUAGE:C,CXX>:/wd4324> # Structure was padded due to alignment specifier.
        $<$<COMPILE_LANGUAGE:C,CXX>:/wd4351> # New behavior: elements of array will be default initialized.
        $<$<COMPILE_LANGUAGE:C,CXX>:/wd4577> # 'noexcept' used with no exception handling mode specified.
        $<$<COMPILE_LANGUAGE:C,CXX>:/wd4996> # 'X' was declared deprecated.
    )
elseif(MINGW)
    # redirect to wmain
    # FIXME: cmake 3.13 added target_link_options
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -municode")
endif()
add_library(crashpad::interface ALIAS crashpad_interface)

add_subdirectory(compat)
add_subdirectory(minidump)
add_subdirectory(snapshot)
add_subdirectory(util)
add_subdirectory(third_party/mini_chromium)
add_subdirectory(client)

add_subdirectory(third_party/zlib)
add_subdirectory(third_party/getopt)

add_subdirectory(tools)
add_subdirectory(handler)

if(CRASHPAD_ENABLE_INSTALL_DEV)
    install(EXPORT crashpad_export NAMESPACE crashpad:: FILE crashpad-targets.cmake
        DESTINATION "${CMAKE_INSTALL_CMAKEDIR}")
    include(CMakePackageConfigHelpers)
    configure_package_config_file(crashpad-config.cmake.in crashpad-config.cmake
        INSTALL_DESTINATION "${CMAKE_INSTALL_CMAKEDIR}"
    )
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/crashpad-config.cmake" DESTINATION "${CMAKE_INSTALL_CMAKEDIR}")
endif()