cmake_minimum_required(VERSION 3.14)

project(sort_ade LANGUAGES C)

include(GNUInstallDirs)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -Wall -Wextra -pedantic")

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

add_executable(${PROJECT_NAME} ${PROJECT_NAME}.c)

install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT core
)