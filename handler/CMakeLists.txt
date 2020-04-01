add_executable(crashpad_handler WIN32
    main.cc
    crash_report_upload_thread.cc
    crash_report_upload_thread.h
    handler_main.cc
    handler_main.h
    minidump_to_upload_parameters.cc
    minidump_to_upload_parameters.h
    prune_crash_reports_thread.cc
    prune_crash_reports_thread.h
    user_stream_data_source.cc
    user_stream_data_source.h
)

if(APPLE)
    target_sources(crashpad_handler PRIVATE
        mac/crash_report_exception_handler.cc
        mac/crash_report_exception_handler.h
        mac/exception_handler_server.cc
        mac/exception_handler_server.h
        mac/file_limit_annotation.cc
        mac/file_limit_annotation.h
    )
endif()

if(LINUX OR ANDROID)
    target_sources(crashpad_handler PRIVATE
        linux/capture_snapshot.cc
        linux/capture_snapshot.h
        linux/crash_report_exception_handler.cc
        linux/crash_report_exception_handler.h
        linux/exception_handler_server.cc
        linux/exception_handler_server.h
    )
endif()

if(LINUX)
    target_sources(crashpad_handler PRIVATE
        linux/cros_crash_report_exception_handler.cc
        linux/cros_crash_report_exception_handler.h
    )
endif()

if(WIN32)
    target_sources(crashpad_handler PRIVATE
        win/crash_report_exception_handler.cc
        win/crash_report_exception_handler.h
    )
endif()

target_link_libraries(crashpad_handler
    PRIVATE
        $<BUILD_INTERFACE:crashpad_interface>
    PUBLIC
        crashpad_client
        crashpad_getopt
        crashpad_minidump
        crashpad_snapshot
        crashpad_tools
        crashpad_util
        mini_chromium
)

if(WIN32) 
    if(MSVC)
        target_compile_options(crashpad_handler PRIVATE "/wd4201")
    endif()
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_compile_options(crashpad_handler PRIVATE 
            "-Wno-multichar"
        )
    endif()
endif()

set_property(TARGET crashpad_handler PROPERTY EXPORT_NAME handler)
add_executable(crashpad::handler ALIAS crashpad_handler)

install(TARGETS crashpad_handler EXPORT crashpad_export
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)
