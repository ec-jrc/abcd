cmake_minimum_required(VERSION 3.14)

project(abcd VERSION 1.0.0)

option(BUILD_ABSP "Build also the absp module, that interfaces SP Devices digitizers." OFF)

include(GNUInstallDirs)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(ABCD_DATADIR ${CMAKE_INSTALL_DATADIR}/abcd)
set(ABCD_FULL_DATADIR ${CMAKE_INSTALL_FULL_DATADIR}/abcd)

set(REQUIRED_EXECUTABLES "tmux")

foreach(executable ${REQUIRED_EXECUTABLES})
    find_program(executable_found ${executable} NO_CACHE)
    if(NOT executable_found)
        message(FATAL_ERROR "'${executable}' is required but not found. Please install it.")
    endif()
endforeach()

find_package(Python3 REQUIRED COMPONENTS Interpreter)
if(NOT Python3_FOUND)
    message(FATAL_ERROR "Python3 with is required but not found. Please install it.")
endif()

set(REQUIRED_PYTHON_PACKAGES "numpy" "scipy" "zmq")

foreach(python_package ${REQUIRED_PYTHON_PACKAGES})
    execute_process(
        COMMAND ${Python3_EXECUTABLE} -c "import ${python_package}"
        RESULT_VARIABLE PYTHON_PACKAGE_FOUND
        OUTPUT_QUIET
        ERROR_QUIET
    )
    if(NOT PYTHON_PACKAGE_FOUND EQUAL 0)
        message(FATAL_ERROR "Python package '${python_package}' is required but not found. Please install it.")
    endif()
endforeach()

include_directories(
    include
)

set(ABCD_HEADERS include/events.h)

add_library(abcd_headers INTERFACE "${ABCD_HEADERS}")

set_target_properties(abcd_headers
    PROPERTIES PUBLIC_HEADER ${ABCD_HEADERS}
)

install(TARGETS abcd_headers
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/abcd
    COMPONENT core
)

install(PROGRAMS
    bin/send_command.py
    bin/log_status_events.py
    bin/split_events_files.py
    bin/fan-in.py
    bin/fan-out.py
    bin/save_to_file.py
    bin/close_file.py
    bin/plot_Evst.py
    bin/plot_PSD.py
    bin/plot_spectra.py
    bin/plot_timestamps.py
    bin/plot_ToF.py
    bin/plot_waveforms.py
    replay/replay_raw.py
    replay/replay_raw
    replay/replay_events
    convert/adr2adeadw.py
    convert/ade2ascii
    convert/ade2ascii.m
    convert/ade2ascii.py
    convert/adr2ade
    convert/adr2events.py
    convert/adr2configs.py
    convert/adw2ascii
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT core
)

file(GLOB STARTUPS_IN "${CMAKE_CURRENT_SOURCE_DIR}/startup/*.sh.in")
file(GLOB STARTUPS "${CMAKE_CURRENT_SOURCE_DIR}/startup/*.sh")

file(MAKE_DIRECTORY ${ABCD_DATADIR}/startup)

foreach(startup_in ${STARTUPS_IN})
    get_filename_component(startup_name ${startup_in} NAME)
    string(REPLACE ".in" "" startup ${startup_name})

    configure_file(
        ${startup_in}
        "${CMAKE_CURRENT_BINARY_DIR}/startup/${startup}"
        @ONLY
    )

    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/startup/${startup}"
        COMPONENT core
        DESTINATION ${ABCD_DATADIR}/startup
    )
endforeach()

foreach(startup ${STARTUPS})
    install(FILES ${startup}
        DESTINATION ${ABCD_DATADIR}/startup
        COMPONENT core
    )
endforeach()

install(DIRECTORY examples
    DESTINATION ${CMAKE_INSTALL_DATADIR}/abcd
    COMPONENT core
)

install(DIRECTORY data
    DESTINATION ${CMAKE_INSTALL_DATADIR}/abcd
    COMPONENT core
    FILES_MATCHING PATTERN "example_data_*"
)

add_subdirectory(wit)
add_subdirectory(wadi)

add_subdirectory(dasa)
add_subdirectory(spec)
add_subdirectory(tofcalc)

add_subdirectory(califo)
add_subdirectory(chafi)
add_subdirectory(enfi)
add_subdirectory(cofi)
add_subdirectory(sofi)

add_subdirectory(sort_ade)
add_subdirectory(filter_timestamps)
add_subdirectory(events_counter)

if(BUILD_ABSP)
    message(STATUS "Building absp module")
    add_subdirectory(absp)
endif()

# Better keep waan as the last module as it warns the user that ldconfig should
# be called after a manual installation. If placed before wit, the warning would
# be flooded by the installation of the node_modules folder
add_subdirectory(waan)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/deb")

include(CPackConfiguration)