cmake_minimum_required(VERSION 3.14)

project(absp VERSION 1.0.0 LANGUAGES CXX)

include(GNUInstallDirs)

if(NOT DEFINED ABCD_DATADIR)
    set(ABCD_DATADIR ${CMAKE_INSTALL_DATADIR}/abcd)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -Wall -Wextra -pedantic")

find_package(SWIG 4.2 REQUIRED COMPONENTS lua)
include(UseSWIG)

find_path(ZMQ_INCLUDE_DIR NAMES zmq.h)
find_library(ZMQ_LIBRARY NAMES zmq)

find_path(JANSSON_INCLUDE_DIR NAMES jansson.h)
find_library(JANSSON_LIBRARY NAMES jansson)

# This one generates the wrong LUA_INCLUDE_DIRS
# (e.g. it gives /usr/include instead of /usr/include/lua5.4)
#find_package(Lua 5.4 REQUIRED)
find_path(LUA_INCLUDE_DIR
    NAMES lua.hpp
    PATH_SUFFIXES "lua5.4"
)
find_library(LUA_LIBRARY NAMES lua5.4)

find_path(ADQ_INCLUDE_DIR NAMES ADQAPI.h)
find_library(ADQ_LIBRARY NAMES adq)

include_directories(
    include
    ../include
)

set(DIGITIZER_MODULES_SOURCES
    src/ADQ214.cpp
    src/ADQ412.cpp
    src/ADQ14_FWDAQ.cpp
    src/ADQ14_FWPD.cpp
    src/ADQ36_FWDAQ.cpp
)

set(SOURCES
    src/ADQ_descriptions.cpp
    src/actions.cpp
    src/states.cpp
    ${DIGITIZER_MODULES_SOURCES}
)

file(GLOB ABSP_CONFIGS "${CMAKE_CURRENT_SOURCE_DIR}/configs/*.json")
file(GLOB ABSP_SCRIPTS "${CMAKE_CURRENT_SOURCE_DIR}/scripts/*")

set(SWIG_RUNTIME_HEADER
    ${CMAKE_CURRENT_BINARY_DIR}/include/swigluarun.h
)
get_filename_component(SWIG_RUNTIME_DIR ${SWIG_RUNTIME_HEADER} DIRECTORY)
file(MAKE_DIRECTORY ${SWIG_RUNTIME_DIR})

add_custom_command(
    OUTPUT ${SWIG_RUNTIME_HEADER}
    COMMAND ${SWIG_EXECUTABLE} -c++ -lua -external-runtime ${SWIG_RUNTIME_HEADER}
    COMMENT "Generating SWIG runtime header for lua: ${SWIG_RUNTIME_HEADER}"
)

add_custom_target(swig_runtime DEPENDS ${SWIG_RUNTIME_HEADER})

set(SWIG_WRAPPERS
    ${CMAKE_CURRENT_BINARY_DIR}/src/Digitizers_wrap.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/src/ADQAPI_wrap.cpp
)

foreach(wrapper ${SWIG_WRAPPERS})
    get_filename_component(SWIG_WRAPPER_DIR ${wrapper} DIRECTORY)
    file(MAKE_DIRECTORY ${SWIG_WRAPPER_DIR})

    get_filename_component(wrapper_name ${wrapper} NAME)

    string(REPLACE "_wrap.cpp" ".i" wrapper_source ${wrapper_name})
    string(PREPEND wrapper_source "${CMAKE_CURRENT_SOURCE_DIR}/src/")

    add_custom_command(
        OUTPUT ${wrapper}
        # The -D__linux__ definition is required by ADQAPI.h
        COMMAND ${SWIG_EXECUTABLE} -c++ -lua -D__linux__ -I${LUA_INCLUDE_DIR} -I${ADQ_INCLUDE_DIR} -I${CMAKE_CURRENT_SOURCE_DIR}/include -I${CMAKE_CURRENT_SOURCE_DIR}/../include -o ${wrapper} ${wrapper_source}
        DEPENDS ${wrapper_source}
        COMMENT "Generating SWIG wrapper for lua with: ${wrapper_source}"
    )
endforeach()

add_library(LuaDigitizers OBJECT ${SWIG_WRAPPERS})

add_dependencies(LuaDigitizers swig_runtime)
target_include_directories(LuaDigitizers PUBLIC ${LUA_INCLUDE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/include)

# FIXME I cannot get this to work:
# It generates a shared library without the luaopen_digitizers() function
#set_source_files_properties(
#    SOURCE include/Digitizers.i
#    PROPERTY CPLUSPLUS ON
#    PROPERTY INCLUDE_DIRECTORIES include ../include
#    PROPERTY USE_SWIG_DEPENDENCIES TRUE
#    PROPERTY USE_TARGET_INCLUDE_DIRECTORIES TRUE
#)
#swig_add_library(digitizers
#    TYPE SHARED
#    LANGUAGE lua
#    SOURCES include/Digitizers.i
#)
#set_target_properties(digitizers PROPERTIES LINKER_LANGUAGE CXX)

add_library(LuaManager INTERFACE include/LuaManager.hpp)
target_include_directories(LuaManager INTERFACE include/)
set_target_properties(LuaManager
    PROPERTIES PUBLIC_HEADER include/LuaManager.hpp
)

add_executable(${PROJECT_NAME} ${SOURCES} ${PROJECT_NAME}.cpp $<TARGET_OBJECTS:LuaDigitizers>)

target_include_directories(${PROJECT_NAME} PUBLIC ${ZMQ_INCLUDE_DIR} ${JANSSON_INCLUDE_DIR} ${ADQ_INCLUDE_DIR} ${LUA_INCLUDE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/include)
target_link_libraries(${PROJECT_NAME} PUBLIC ${ZMQ_LIBRARY} ${JANSSON_LIBRARY} ${ADQ_LIBRARY} ${LUA_LIBRARY} LuaManager)

install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT absp
)

install(FILES ${ABSP_CONFIGS}
    DESTINATION ${ABCD_DATADIR}/${PROJECT_NAME}/configs
    COMPONENT absp
)

install(FILES ${ABSP_SCRIPTS}
    DESTINATION ${ABCD_DATADIR}/${PROJECT_NAME}/scripts
    COMPONENT absp
)
