cmake_minimum_required(VERSION 3.14)

project(califo LANGUAGES CXX)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -Wall -Wextra -pedantic")

find_package(GSL REQUIRED)
find_package(PkgConfig REQUIRED)

pkg_check_modules(ZMQ REQUIRED libzmq)
pkg_check_modules(JANSSON REQUIRED jansson)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

set(EXEC califo)
set(SOURCES
    src/actions.cpp
    src/states.cpp
    src/background.cpp
)
file(GLOB CONFIGS "${CMAKE_CURRENT_SOURCE_DIR}/configs/*.json")

add_executable(${EXEC} ${SOURCES} ${EXEC}.cpp)

target_include_directories(${EXEC} PUBLIC ${GSL_INCLUDE_DIRS} ${ZMQ_INCLUDE_DIRS} ${JANSSON_INCLUDE_DIRS})
target_link_libraries(${EXEC} PUBLIC ${GSL_LIBRARIES} ${ZMQ_LIBRARIES} ${JANSSON_LIBRARIES})

install(TARGETS ${EXEC}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES ${CONFIGS} DESTINATION ${CMAKE_INSTALL_DATADIR}/abcd/${EXEC})