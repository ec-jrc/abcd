cmake_minimum_required(VERSION 3.14)

if(CMAKE_VERSION VERSION_LESS "3.14")
    message(FATAL_ERROR "CMake >= 3.14 is required. You are running ${CMAKE_VERSION}")
endif()

project(ABCD VERSION 1.0.0)

include(GNUInstallDirs)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(REQUIRED_EXECUTABLES "tmux")

foreach(executable ${REQUIRED_EXECUTABLES})
    find_program(EXECUTABLE_FOUND ${executable})
    if(NOT EXECUTABLE_FOUND)
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

add_library(ABCD INTERFACE
    "${ABCD_HEADERS}"
)

set_target_properties(ABCD
    PROPERTIES PUBLIC_HEADER ${ABCD_HEADERS}
)

install(TARGETS ABCD
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/abcd
)

install(PROGRAMS
    bin/send_command.py
    bin/log_status_events.py
    bin/filter_timestamps
    bin/sort_ade
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
)

install(DIRECTORY startup
    DESTINATION ${CMAKE_INSTALL_DATADIR}/abcd/startup
    FILES_MATCHING PATTERN "*.sh"
)

install(DIRECTORY examples
    DESTINATION ${CMAKE_INSTALL_DATADIR}/abcd/examples
)

install(DIRECTORY data
    DESTINATION ${CMAKE_INSTALL_DATADIR}/abcd/data
    FILES_MATCHING PATTERN "example_data_*"
)

add_subdirectory(wit)
add_subdirectory(dasa)
add_subdirectory(califo)
add_subdirectory(spec)
add_subdirectory(tofcalc)

# Better keep waan as the last module as it warns the user that ldconfig should
# be called after a manual installation. If placed before wit, the warning would
# be flooded by the installation of the node_modules folder
add_subdirectory(waan)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/deb")
include(CPackConfig)