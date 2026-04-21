cmake_minimum_required(VERSION 4.0)
project(Buckshot_roulette C)

set(CMAKE_C_STANDARD 23)

find_package(Curses REQUIRED)
include_directories(${CURSES_INCLUDE_DIR})

add_executable(Buckshot_roulette main.c
        screens.h
        screens.c
        ui.c
        gameplay.c)

target_link_libraries(Buckshot_roulette ${CURSES_LIBRARIES})