cmake_minimum_required(VERSION 3.14)

project(events_counter LANGUAGES CXX)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -Wall -Wextra -pedantic")

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

add_executable(${PROJECT_NAME} ${PROJECT_NAME}.cpp)

install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT core
)
