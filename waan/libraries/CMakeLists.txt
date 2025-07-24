cmake_minimum_required(VERSION 3.14)

project(waan_libraries LANGUAGES C)

include(GNUInstallDirs)

if(NOT DEFINED ABCD_DATADIR)
    set(ABCD_DATADIR ${CMAKE_INSTALL_DATADIR}/abcd)
endif()

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
)

find_package(GSL REQUIRED)

find_path(JANSSON_INCLUDE_DIR NAMES jansson.h)
find_library(JANSSON_LIBRARY NAMES jansson)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99 -O3 -Wall -Wextra -pedantic")

set(IMPLICIT_INCLUDE_DIRECTORIES "")

foreach(dir ${CMAKE_C_IMPLICIT_INCLUDE_DIRECTORIES})
    string(APPEND IMPLICIT_INCLUDE_DIRECTORIES "-I${dir} ")
endforeach()

string(APPEND IMPLICIT_INCLUDE_DIRECTORIES "-I${CMAKE_INSTALL_FULL_INCLUDEDIR}/abcd/ ")

set(IMPLICIT_LINK_DIRECTORIES "")

foreach(dir ${CMAKE_C_IMPLICIT_LINK_DIRECTORIES})
    string(APPEND IMPLICIT_LINK_DIRECTORIES "-L${dir} ")
endforeach()

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/Makefile.in
    ${CMAKE_CURRENT_BINARY_DIR}/Makefile_waan_libraries
    @ONLY
)

file(GLOB HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h")
file(GLOB SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c")

foreach(source_file ${SOURCES})
    get_filename_component(lib_name ${source_file} NAME_WE)

    add_library(${lib_name} SHARED ${source_file})

    set_target_properties(${lib_name} PROPERTIES PREFIX "")

    target_include_directories(${lib_name} PUBLIC ${GSL_INCLUDE_DIRS} ${JANSSON_INCLUDE_DIR})

    install(TARGETS ${lib_name}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/abcd/
        COMPONENT core
    )
endforeach()

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/Makefile_waan_libraries
    DESTINATION ${ABCD_DATADIR}/waan_libraries
    RENAME Makefile
    COMPONENT core
)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/../../include/events.h
    DESTINATION ${ABCD_DATADIR}/waan_libraries/include
    COMPONENT core
)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/../include/analysis_functions.h
    DESTINATION ${ABCD_DATADIR}/waan_libraries/include
    COMPONENT core
)
install(FILES ${HEADERS}
    DESTINATION ${ABCD_DATADIR}/waan_libraries/include
    COMPONENT core
)
install(FILES ${SOURCES}
    DESTINATION ${ABCD_DATADIR}/waan_libraries/src
    COMPONENT core
)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/abcd.conf.in
        ${CMAKE_CURRENT_BINARY_DIR}/abcd.conf
        @ONLY
    )

    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/abcd.conf
        DESTINATION /etc/ld.so.conf.d/
        COMPONENT core
    )

    install(CODE "message(WARNING \"If manually installing, the libraries location must be configured with ldconfig after the installation\")")
endif()
