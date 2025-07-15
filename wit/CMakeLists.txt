cmake_minimum_required(VERSION 3.14)

project(wit LANGUAGES C)

option(FORCE_NPM_INSTALL "Force 'npm install' execution, even if node_modules/ directory already exists" OFF)

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

file(GLOB NODE_MODULES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/node_modules")

if(NOT EXISTS ${NODE_MODULES_DIR} OR FORCE_NPM_INSTALL)
    add_custom_target(wit_install ALL
        COMMAND npm install
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        RESULT_VARIABLE result
        COMMENT "Installing Node.js dependencies in ${CMAKE_CURRENT_SOURCE_DIR}"
    )

    if(NOT result EQUAL 0)
        message(FATAL_ERROR "npm install failed with exit code ${result}")
    endif()
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
    COMPONENT core
    DESTINATION ${CMAKE_INSTALL_DATADIR}/abcd/wit
)
