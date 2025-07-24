cmake_minimum_required(VERSION 3.14)

project(replay LANGUAGES C)

include(GNUInstallDirs)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -Wall -Wextra -pedantic")

find_path(ZMQ_INCLUDE_DIR NAMES zmq.h)
find_library(ZMQ_LIBRARY NAMES zmq)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

add_executable(replay_events replay_events.c)

target_include_directories(replay_events PUBLIC ${ZMQ_INCLUDE_DIR})
target_link_libraries(replay_events PUBLIC ${ZMQ_LIBRARY})

install(TARGETS replay_events
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT core
)
