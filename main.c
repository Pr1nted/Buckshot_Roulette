#include "types.h"
#include "ui.h"
#include "screens.h"

// --- Global Definitions ---
int active_loadout = 0;
int g_flashes_enabled = 0;

const char *loadout_names[] = {
    "Classic",
    "Double or nothing",
    "Pr1nted loadout+"
};

WINDOW *border_win;

// --- Init Helpers ---

void init_colors(void) {
    start_color();
    init_pair(0, COLOR_WHITE, COLOR_BLACK);
    init_pair(1, COLOR_RED, COLOR_BLACK);
    init_pair(2, COLOR_YELLOW, COLOR_BLACK);
    init_pair(3, COLOR_GREEN, COLOR_BLACK);
    init_pair(4, COLOR_BLUE, COLOR_BLACK);
    init_pair(7, COLOR_BLACK, COLOR_WHITE); // FLASH PAIR

    // Apply background to entire stdscr
    wbkgd(stdscr, COLOR_PAIR(0));
    wbkgd(border_win, COLOR_PAIR(0));
}

// --- Main Entry Point ---

int main(void) {
    srand(time(NULL));
    initscr();
    init_colors();

    clear();
    refresh();
    noecho();
    cbreak();
    keypad(stdscr, TRUE);

    // Create border
    border_win = newwin(LINES, COLS, 0, 0);

    // Needed to display the menu without first input
    wrefresh(stdscr);

    const char *main_title[] = {
        "Buckshot Roulette",
        "",
        "written by Pr1nted",
        "heavily inspired by work of Mike Klubinka"
    };
    const char *main_options[] = { "Start Game", "Loadouts", "Settings", "Credits", "Quit" };
    Menu main_menu = {
        .title_lines   = main_title,
        .n_title_lines = 4,
        .options       = main_options,
        .n_options     = 5
    };

    while (1) {
        int choice = create_menu(main_menu);

        if (choice == 0) {
            create_start_game_menu();
        } else if (choice == 1) {
            create_loadouts();
        } else if (choice == 2) {
            create_settings();
        } else if (choice == 3) {
            create_credits();
        } else if (choice == 4) {
            break;
        }
    }

    delwin(border_win);
    endwin();
    return 0;
}