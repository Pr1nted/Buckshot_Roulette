cmake_minimum_required(VERSION 3.10)
project(Buckshot_roulette C)

set(CMAKE_C_STANDARD 11)

find_package(Curses REQUIRED)
include_directories(${CURSES_INCLUDE_DIR})

find_package(Threads REQUIRED)

add_executable(Buckshot_roulette
        main.c
        screens.h
        screens.c
        ui.c
        gameplay.c
        network.h
        network.c
)

target_link_libraries(Buckshot_roulette ${CURSES_LIBRARIES})

# Link threading library
target_link_libraries(Buckshot_roulette Threads::Threads)

if(WIN32)
    target_link_libraries(Buckshot_roulette ws2_32 iphlpapi) # Added iphlpapi
endif()