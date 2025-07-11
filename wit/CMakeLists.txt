cmake_minimum_required(VERSION 3.14)

project(wit LANGUAGES C)

include(GNUInstallDirs)

set(REQUIRED_EXECUTABLES "node" "npm")

foreach(executable ${REQUIRED_EXECUTABLES})
    find_program(EXECUTABLE_FOUND ${executable})
    if(NOT EXECUTABLE_FOUND)
        message(FATAL_ERROR "'${executable}' is required but not found. Please install it.")
    endif()
endforeach()

set(WIT_INSTALLDIR ${CMAKE_INSTALL_FULL_LIBEXECDIR}/abcd/wit)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/wit.sh.in
    ${CMAKE_CURRENT_BINARY_DIR}/wit.sh
    @ONLY
)

add_custom_target(wit_install ALL
    COMMAND npm install
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Installing Node.js dependencies in ${CMAKE_CURRENT_SOURCE_DIR}"
)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
    DESTINATION ${CMAKE_INSTALL_LIBEXECDIR}/abcd/wit
    PATTERN "CMakeLists.txt" EXCLUDE
    PATTERN "wit.sh.in" EXCLUDE
    PATTERN ".gitignore" EXCLUDE
    PATTERN "config_coincidences.json" EXCLUDE
)

install(PROGRAMS
    ${CMAKE_CURRENT_BINARY_DIR}/wit.sh
    DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/config_coincidences.json" DESTINATION ${CMAKE_INSTALL_DATADIR}/abcd/wit)