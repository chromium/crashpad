list(APPEND MINI_CHROMIUM_SOURCES
    ../build/build_config.h
    atomicops.h
    atomicops_internals_atomicword_compat.h
    atomicops_internals_portable.h
    auto_reset.h
    bit_cast.h
    compiler_specific.h
    debug/alias.cc
    debug/alias.h
    files/file_path.cc
    files/file_path.h
    files/file_util.h
    files/scoped_file.cc
    files/scoped_file.h
    format_macros.h
    logging.cc
    logging.h
    macros.h
    memory/free_deleter.h
    memory/scoped_policy.h
    metrics/histogram_functions.h
    metrics/histogram_macros.h
    metrics/persistent_histogram_allocator.h
    numerics/checked_math.h
    numerics/checked_math_impl.h
    numerics/clamped_math.h
    numerics/clamped_math_impl.h
    numerics/safe_conversions.h
    numerics/safe_conversions_arm_impl.h
    numerics/safe_conversions_impl.h
    numerics/safe_math.h
    numerics/safe_math_arm_impl.h
    numerics/safe_math_clang_gcc_impl.h
    numerics/safe_math_shared_impl.h
    process/memory.cc
    process/memory.h
    process/process_metrics.h
    rand_util.cc
    rand_util.h
    scoped_clear_errno.h
    scoped_generic.h
    stl_util.h
    strings/string16.cc
    strings/string16.h
    strings/string_number_conversions.cc
    strings/string_number_conversions.h
    strings/string_piece.h
    strings/string_util.h
    strings/stringprintf.cc
    strings/stringprintf.h
    strings/sys_string_conversions.h
    strings/utf_string_conversion_utils.cc
    strings/utf_string_conversion_utils.h
    strings/utf_string_conversions.cc
    strings/utf_string_conversions.h
    synchronization/condition_variable.h
    synchronization/lock.cc
    synchronization/lock.h
    synchronization/lock_impl.h
    sys_byteorder.h
    template_util.h
    third_party/icu/icu_utf.cc
    third_party/icu/icu_utf.h
    threading/thread_local_storage.cc
    threading/thread_local_storage.h
)

if(APPLE)
    list(APPEND MINI_CHROMIUM_SOURCES
        mac/close_nocancel.cc
        mac/foundation_util.h
        mac/foundation_util.mm
        mac/mach_logging.cc
        mac/mach_logging.h
        mac/scoped_cftyperef.h
        mac/scoped_ioobject.h
        mac/scoped_launch_data.h
        mac/scoped_mach_port.cc
        mac/scoped_mach_port.h
        mac/scoped_mach_vm.cc
        mac/scoped_mach_vm.h
        mac/scoped_nsautorelease_pool.h
        mac/scoped_nsautorelease_pool.mm
        mac/scoped_nsobject.h
        mac/scoped_typeref.h
        strings/sys_string_conversions_mac.mm
    )
endif()

if(WIN32)
    list(APPEND MINI_CHROMIUM_SOURCES
        process/process_metrics_win.cc
        strings/string_util_win.cc
        strings/string_util_win.h
        synchronization/lock_impl_win.cc
        threading/thread_local_storage_win.cc
    )
else()
    list(APPEND MINI_CHROMIUM_SOURCES
        files/file_util_posix.cc
        posix/eintr_wrapper.h
        posix/safe_strerror.cc
        posix/safe_strerror.h
        process/process_metrics_posix.cc
        strings/string_util_posix.h
        synchronization/condition_variable_posix.cc
        synchronization/lock_impl_posix.cc
        threading/thread_local_storage_posix.cc
    )
endif()

list(TRANSFORM MINI_CHROMIUM_SOURCES PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/mini_chromium/base/")

add_library(mini_chromium STATIC ${MINI_CHROMIUM_SOURCES})

if(APPLE)
    target_link_libraries(mini_chromium PUBLIC "-framework ApplicationServices")
    target_link_libraries(mini_chromium PUBLIC "-framework CoreFoundation")
    target_link_libraries(mini_chromium PUBLIC "-framework Foundation")
    target_link_libraries(mini_chromium PUBLIC "-framework IOKit")
    target_link_libraries(mini_chromium PUBLIC "-framework Security")
endif()

if(WIN32)
    target_link_libraries(mini_chromium "advapi32" "kernel32")
    target_compile_options(mini_chromium PUBLIC "/wd4201" "/wd4996")
    target_compile_definitions(mini_chromium PUBLIC
        NOMINMAX
        UNICODE
        WIN32_LEAN_AND_MEAN
        _CRT_SECURE_NO_WARNINGS
        _HAS_EXCEPTIONS=0
        _UNICODE
    )
endif()
