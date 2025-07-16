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

add_executable(ade2ascii ade2ascii.c)
add_executable(adr2ade adr2ade.c)
add_executable(adw2ascii adw2ascii.c)

install(TARGETS ade2ascii
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT core
)
install(TARGETS adr2ade
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT core
)
install(TARGETS adw2ascii
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT core
)