cmake_minimum_required(VERSION 3.14)

project(califo LANGUAGES CXX)

include(GNUInstallDirs)

if(NOT DEFINED ABCD_DATADIR)
    set(ABCD_DATADIR ${CMAKE_INSTALL_DATADIR}/abcd)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -Wall -Wextra -pedantic")

find_package(GSL REQUIRED)

find_path(ZMQ_INCLUDE_DIR NAMES zmq.h)
find_library(ZMQ_LIBRARY NAMES zmq)

find_path(JANSSON_INCLUDE_DIR NAMES jansson.h)
find_library(JANSSON_LIBRARY NAMES jansson)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

set(SOURCES
    src/actions.cpp
    src/states.cpp
    src/background.cpp
)
file(GLOB CONFIGS "${CMAKE_CURRENT_SOURCE_DIR}/configs/*.json")

add_executable(${PROJECT_NAME} ${SOURCES} ${PROJECT_NAME}.cpp)

target_include_directories(${PROJECT_NAME} PUBLIC ${GSL_INCLUDE_DIRS} ${ZMQ_INCLUDE_DIR} ${JANSSON_INCLUDE_DIR})
target_link_libraries(${PROJECT_NAME} PUBLIC ${GSL_LIBRARIES} ${ZMQ_LIBRARY} ${JANSSON_LIBRARY})

install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT core
)

install(FILES ${CONFIGS}
    DESTINATION ${ABCD_DATADIR}/${PROJECT_NAME}
    COMPONENT core
)
