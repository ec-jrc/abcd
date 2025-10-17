cmake_minimum_required(VERSION 3.14)

project(convert LANGUAGES C)

include(GNUInstallDirs)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -Wall -Wextra -pedantic")

find_path(ZMQ_INCLUDE_DIR NAMES zmq.h)
find_library(ZMQ_LIBRARY NAMES zmq)

find_path(JANSSON_INCLUDE_DIR NAMES jansson.h)
find_library(JANSSON_LIBRARY NAMES jansson)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

set(EXECUTABLES chafi cofi enfi sofi pufi)

foreach(executable ${EXECUTABLES})
    add_executable(${executable} ${executable}.c)

    target_include_directories(${executable} PUBLIC ${ZMQ_INCLUDE_DIR} ${JANSSON_INCLUDE_DIR})
    target_link_libraries(${executable} PUBLIC m ${ZMQ_LIBRARY} ${JANSSON_LIBRARY})

    install(TARGETS ${executable}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        COMPONENT core
    )
endforeach()
