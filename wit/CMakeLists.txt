cmake_minimum_required(VERSION 3.20)

project(wit LANGUAGES C)

option(FORCE_NPM_INSTALL "Force 'npm install' execution, even if node_modules/ directory already exists. It requires an internet connection at build phase." OFF)

include(GNUInstallDirs)

if(NOT DEFINED ABCD_DATADIR)
    set(ABCD_DATADIR ${CMAKE_INSTALL_DATADIR}/abcd)
endif()

set(REQUIRED_EXECUTABLES "node" "npm")

foreach(executable ${REQUIRED_EXECUTABLES})
    find_program(executable_found ${executable} NO_CACHE)
    if(NOT executable_found)
        message(FATAL_ERROR "'${executable}' is required but not found. Please install it.")
    endif()
endforeach()

set(WIT_INSTALLDIR ${CMAKE_INSTALL_FULL_LIBEXECDIR}/abcd/wit)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/wit.sh.in
    ${CMAKE_CURRENT_BINARY_DIR}/wit.sh
    @ONLY
)

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/node_modules" OR FORCE_NPM_INSTALL)
    add_custom_target(npm_install ALL
        COMMAND npm install
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        COMMENT "Installing Node.js dependencies in ${CMAKE_CURRENT_SOURCE_DIR}"
    )
else()
    message(STATUS "Skipping 'npm install' as node_modules/ directory already exists. Use 'cmake -DFORCE_NPM_INSTALL=ON ...' to force it.")
endif()

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
    DESTINATION ${CMAKE_INSTALL_LIBEXECDIR}/abcd/wit
    COMPONENT core
    PATTERN "CMakeLists.txt" EXCLUDE
    PATTERN "wit.sh.in" EXCLUDE
    PATTERN ".gitignore" EXCLUDE
    PATTERN "config_coincidences.json" EXCLUDE
)

install(PROGRAMS
    ${CMAKE_CURRENT_BINARY_DIR}/wit.sh
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT core
)

install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/config_coincidences.json"
    DESTINATION ${ABCD_DATADIR}/wit
    COMPONENT core
)
