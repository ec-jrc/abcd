cmake_minimum_required(VERSION 3.14)

project(waan LANGUAGES CXX)

include(GNUInstallDirs)

add_subdirectory(libraries)

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

set(EXEC waan)
set(SOURCES
    src/actions.cpp
    src/states.cpp
)
file(GLOB CONFIGS "${CMAKE_CURRENT_SOURCE_DIR}/configs/*.json")

add_executable(${EXEC} ${SOURCES} ${EXEC}.cpp)

target_include_directories(${EXEC} PUBLIC ${ZMQ_INCLUDE_DIRS} ${JANSSON_INCLUDE_DIRS})
target_link_libraries(${EXEC} PUBLIC dl ${ZMQ_LIBRARIES} ${JANSSON_LIBRARIES})

install(TARGETS ${EXEC}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES ${CONFIGS} DESTINATION ${CMAKE_INSTALL_DATADIR}/abcd/${EXEC})