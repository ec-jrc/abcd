cmake_minimum_required(VERSION 3.14)

project(spec LANGUAGES CXX)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -Wall -Wextra -pedantic")

find_package(PkgConfig REQUIRED)

pkg_check_modules(ZMQ REQUIRED libzmq)
pkg_check_modules(JANSSON REQUIRED jansson)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

set(SOURCES
    src/actions.cpp
    src/states.cpp
)

add_executable(${PROJECT_NAME} ${SOURCES} ${PROJECT_NAME}.cpp)

target_include_directories(${PROJECT_NAME} PUBLIC ${ZMQ_INCLUDE_DIRS} ${JANSSON_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} PUBLIC ${ZMQ_LIBRARIES} ${JANSSON_LIBRARIES})

install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT core
)
