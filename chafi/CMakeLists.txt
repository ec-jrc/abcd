cmake_minimum_required(VERSION 3.14)

project(chafi LANGUAGES C)

include(GNUInstallDirs)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -Wall -Wextra -pedantic")

find_package(PkgConfig REQUIRED)

pkg_check_modules(ZMQ REQUIRED libzmq)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

add_executable(${PROJECT_NAME} ${PROJECT_NAME}.c)

target_include_directories(${PROJECT_NAME} PUBLIC ${ZMQ_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} PUBLIC ${ZMQ_LIBRARIES})

install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT core
)