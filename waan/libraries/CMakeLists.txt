cmake_minimum_required(VERSION 3.14)

project(waan_libraries LANGUAGES C)

include(GNUInstallDirs)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
)

find_package(PkgConfig REQUIRED)
find_package(GSL REQUIRED)

pkg_check_modules(JANSSON REQUIRED jansson)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99 -O3 -Wall -Wextra -pedantic")

file(GLOB SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c")

foreach(source_file ${SOURCES})
    get_filename_component(lib_name ${source_file} NAME_WE)

    add_library(${lib_name} SHARED ${source_file})

    set_target_properties(${lib_name} PROPERTIES PREFIX "")

    install(TARGETS ${lib_name}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/abcd/
    )
endforeach()

install(CODE "file(WRITE /etc/ld.so.conf.d/abcd.conf \"${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/abcd\n\")")
install(CODE "execute_process(COMMAND ldconfig)")