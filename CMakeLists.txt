cmake_minimum_required(VERSION 3.10)
project(Buckshot_roulette C)

set(CMAKE_C_STANDARD 11)

if(DEFINED NCURSES_PREFIX)
    # Use the explicitly provided prefix (passed from autocompile.sh)
    set(CURSES_INCLUDE_DIR "${NCURSES_PREFIX}/include")
    set(CURSES_LIBRARIES   "${NCURSES_PREFIX}/lib/libncurses.a")
    include_directories(${CURSES_INCLUDE_DIR})
else()
    find_package(Curses REQUIRED)
    include_directories(${CURSES_INCLUDE_DIR})
endif()

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