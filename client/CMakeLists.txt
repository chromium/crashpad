list(APPEND CLIENT_SOURCES
    annotation.cc
    annotation.h
    annotation_list.cc
    annotation_list.h
    crash_report_database.cc
    crash_report_database.h
    crashpad_client.h
    crashpad_info.cc
    crashpad_info.h
    prune_crash_reports.cc
    prune_crash_reports.h
    settings.cc
    settings.h
    simple_address_range_bag.h
    simple_string_dictionary.h
    simulate_crash.h
)

if(APPLE)
    list(APPEND CLIENT_SOURCES
        crash_report_database_mac.mm
        crashpad_client_mac.cc
        simulate_crash_mac.cc
        simulate_crash_mac.h
    )
endif()

if(LINUX OR ANDROID)
    list(APPEND CLIENT_SOURCES
      crashpad_client_linux.cc
      simulate_crash_linux.h
      client_argv_handling.cc
      client_argv_handling.h
      crashpad_info_note.S
      crash_report_database_generic.cc
    )
endif()

if(WIN32)
    list(APPEND CLIENT_SOURCES
      crash_report_database_win.cc
      crashpad_client_win.cc
      simulate_crash_win.h
    )
endif()

add_library(crashpad_client STATIC ${CLIENT_SOURCES})
target_link_libraries(crashpad_client
    crashpad_compat
    crashpad_util
    mini_chromium
)

if(WIN32)
    target_link_libraries(crashpad_client "rpcrt4")
    target_compile_options(crashpad_client PUBLIC "/wd4201")
endif()
