cmake_minimum_required(VERSION 3.14)

project(ade_tools LANGUAGES C CXX)

include(GNUInstallDirs)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -Wall -Wextra -pedantic")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -Wall -Wextra -pedantic")

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

set(C_EXECUTABLES sort_ade)
set(CXX_EXECUTABLES events_counter filter_timestamps)

foreach(executable ${C_EXECUTABLES})
    add_executable(${executable} ${executable}.c)

    install(TARGETS ${executable}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        COMPONENT core
    )
endforeach()


foreach(executable ${CXX_EXECUTABLES})
    add_executable(${executable} ${executable}.cpp)

    install(TARGETS ${executable}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        COMPONENT core
    )
endforeach()
