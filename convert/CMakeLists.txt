cmake_minimum_required(VERSION 3.14)

project(convert LANGUAGES C)

include(GNUInstallDirs)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -Wall -Wextra -pedantic")

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

set(EXECUTABLES ade2ascii adw2ascii adr2ade)

foreach(executable ${EXECUTABLES})
    add_executable(${executable} ${executable}.c)

    install(TARGETS ${executable}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        COMPONENT core
    )
endforeach()
