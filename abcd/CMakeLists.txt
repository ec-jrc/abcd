cmake_minimum_required(VERSION 3.14)

project(abcd VERSION 0.1.0 LANGUAGES CXX)

set (CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

include(GNUInstallDirs)

if(NOT DEFINED ABCD_DATADIR)
    set(ABCD_DATADIR ${CMAKE_INSTALL_DATADIR}/abcd)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -Wall -Wextra -pedantic")

find_package(CAEN REQUIRED)

find_package(PkgConfig REQUIRED)

pkg_check_modules(ZMQ REQUIRED libzmq)
pkg_check_modules(JSONCPP REQUIRED jsoncpp)

include_directories(
    include
    ../include
)

set(SOURCES
    ../src/socket_functions.cpp
    ../src/utilities_functions.cpp
    src/class_caen_dgtz.cpp
    src/actions.cpp
    src/states.cpp
)

file(GLOB ABCD_CONFIGS "${CMAKE_CURRENT_SOURCE_DIR}/configs/*.json")

add_executable(${PROJECT_NAME} ${SOURCES} ${PROJECT_NAME}.cpp)

target_include_directories(${PROJECT_NAME} PUBLIC ${ZMQ_INCLUDE_DIRS} ${JSONCPP_INCLUDE_DIRS} ${CAEN_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} PUBLIC ${ZMQ_LIBRARIES} ${JSONCPP_LIBRARIES} ${CAEN_LIBRARIES})

install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT abcd
)

install(FILES ${ABCD_CONFIGS}
    DESTINATION ${ABCD_DATADIR}/${PROJECT_NAME}
    COMPONENT abcd
)