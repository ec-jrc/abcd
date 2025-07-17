cmake_minimum_required(VERSION 3.14)

project(convert LANGUAGES C)

include(GNUInstallDirs)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -Wall -Wextra -pedantic")

find_package(ZLIB)
find_package(BZip2)
find_package(PkgConfig REQUIRED)

pkg_check_modules(ZMQ REQUIRED libzmq)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

set(EXECUTABLES gzad unzad)

foreach(executable ${EXECUTABLES})
    add_executable(${executable} ${executable}.c)

    target_include_directories(${executable} PUBLIC ${ZMQ_INCLUDE_DIRS} ${ZLIB_INCLUDE_DIRS} ${BZIP2_INCLUDE_DIRS})
    target_link_libraries(${executable} PUBLIC m ${ZMQ_LIBRARIES} ${ZLIB_LIBRARIES} ${BZIP2_LIBRARIES})

    install(TARGETS ${executable}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        COMPONENT core
    )
endforeach()
