#include "types.h"
#include "ui.h"
#include "gameplay.h"
#include "screens.h"

// Helper for game loop delay
void refresh_dealer_view(GameState *gs, WINDOW *gwin) {
    draw_main_ui(gwin, gs);
    napms(1000);
}

// Specific Menu Screens

void create_settings(void) {
    const char *title_str = "Settings";
    int title_h = 3;
    int title_w = 20;

    const char *options[] = {
        "Flashes",
        "Back"
    };
    int n_options = sizeof(options) / sizeof(options[0]);

    int list_w = strlen("Back") + 10;
    int list_h = n_options + 2;

    int title_x = (COLS - title_w) / 2;
    int title_y = (LINES / 2) - (title_h + list_h) / 2;
    int list_x  = (COLS - list_w) / 2;
    int list_y  = title_y + title_h + 1;

    WINDOW *title_win = newwin(title_h, title_w, title_y, title_x);
    WINDOW *list_win  = newwin(list_h,  list_w,  list_y,  list_x);

    int selected = 0;
    int ch;

    while (1) {
        draw_border();

        werase(title_win);
        box(title_win, 0, 0);
        wattron(title_win, A_BOLD | A_UNDERLINE);
        mvwprintw(title_win, 1, (title_w - strlen(title_str)) / 2, "%s", title_str);
        wattroff(title_win, A_BOLD | A_UNDERLINE);
        wrefresh(title_win);

        werase(list_win);
        box(list_win, 0, 0);

        if (selected == 0) wattron(list_win, A_REVERSE);

        int x = (list_w - 12) / 2;

        if (g_flashes_enabled) {
            wattron(list_win, COLOR_PAIR(2) | A_BOLD);
            mvwprintw(list_win, 1, x, "[x]");
            wattroff(list_win, COLOR_PAIR(2) | A_BOLD);
        } else {
            mvwprintw(list_win, 1, x, "[ ]");
        }
        mvwprintw(list_win, 1, x + 4, "%s", options[0]);

        if (selected == 0) wattroff(list_win, A_REVERSE);

        if (selected == 1) wattron(list_win, A_REVERSE);
        mvwprintw(list_win, 2, (list_w - strlen(options[1])) / 2, "%s", options[1]);
        if (selected == 1) wattroff(list_win, A_REVERSE);

        wrefresh(list_win);

        ch = getch();
        switch (ch) {
            case KEY_UP:
            case KEY_DOWN:
                selected = (selected + 1) % n_options;
                break;
            case 10:
                if (selected == 0) {
                    g_flashes_enabled = !g_flashes_enabled;
                } else {
                    goto done;
                }
                break;
        }
    }

done:
    delwin(title_win);
    delwin(list_win);
    clear();
    refresh();
}

void create_loadouts(void) {
    const char *desc_default[] = {
        "The standard loadout that is used in story mode of Buckshot roulette",
        "",
        "Items:",
        "- Beer: Ejects current shell that is loaded in the shotgun",
        "",
        "- Cigarette Pack: Upon being used, the user will gain one charge",
        "",
        "- Handsaw: Doubles the damage of the shell if it is a live round",
        "",
        "- Handcuffs: Skips turn of the selected opponent",
        "",
        "- Magnifying Glass: Reveals the type of round currently loaded in the shotgun"
    };
    const char *desc_heavy[] = {
        "The double or nothing loadout used in Buckshot roulette",
        "",
        "Items:",
        "- Adrenaline: Allows stealing items from opponent",
        "",
        "- Beer: Ejects the current shell loaded in the shotgun",
        "",
        "- Burner phone: Inform the user about the location and type of a random shell loaded in the chamber",
        "",
        "- Cigarette Pack: Upon use, the user gains one charge",
        "",
        "- Expired Medicine: 50% chance of regaining two charges, 50% chance of losing one",
        "",
        "- Handsaw: Doubles the damage of the shell if it is a live round",
        "",
        "- Handcuffs: Skips turn of the selected opponent",
        "",
        "- Inverter: Swaps the current shell into its opposite form",
        "",
        "- Magnifying Glass: Reveals the type of round currently loaded in the shotgun"
    };
    const char *desc_trickster[] = {
        "Work in progress"
    };

    Loadout loadouts[] = {
        { "Classic",           desc_default,   sizeof(desc_default)   / sizeof(desc_default[0])   },
        { "Double or nothing", desc_heavy,     sizeof(desc_heavy)     / sizeof(desc_heavy[0])     },
        { "Pr1nted loadout+",  desc_trickster, sizeof(desc_trickster) / sizeof(desc_trickster[0]) },
    };
    int n_loadouts = sizeof(loadouts) / sizeof(loadouts[0]);

    const char *title_str = "Loadouts";
    int title_h = 3;

    int list_w = strlen("Back");
    for (int i = 0; i < n_loadouts; i++) {
        int len = strlen(loadouts[i].name) + 4;
        if (len > list_w) list_w = len;
    }
    list_w += 4;

    int desc_w  = 40;
    int gap     = 2;
    int total_w = list_w + gap + desc_w;

    int list_x  = (COLS - total_w) / 2;
    int desc_x  = list_x + list_w + gap;
    int title_w = total_w;
    if ((int)strlen(title_str) + 4 > title_w) title_w = strlen(title_str) + 4;
    int title_x = (COLS - title_w) / 2;

    const char *wrapped[3][256];
    int wrapped_counts[3];
    int max_desc_lines = 0;
    for (int i = 0; i < n_loadouts; i++) {
        wrapped_counts[i] = wrap_text_into(
            i,
            loadouts[i].description, loadouts[i].n_desc_lines,
            desc_w, wrapped[i], 256
        );
        if (wrapped_counts[i] > max_desc_lines)
            max_desc_lines = wrapped_counts[i];
    }

    int max_box_h    = LINES - 6;
    int ideal_desc_h = max_desc_lines + 3;
    int desc_h       = (ideal_desc_h > max_box_h) ? max_box_h : ideal_desc_h;

    int list_h_min = 1 + n_loadouts + 1 + 1 + 1;
    int box_h      = (list_h_min > desc_h) ? list_h_min : desc_h;
    if (box_h > max_box_h) box_h = max_box_h;

    int separator_row = box_h - 3;
    int back_row      = box_h - 2;

    int title_y = (LINES - (title_h + 1 + box_h)) / 2;
    int list_y  = title_y + title_h + 1;
    int desc_y  = list_y;

    WINDOW *title_win = newwin(title_h, title_w, title_y, title_x);
    WINDOW *list_win  = newwin(box_h,   list_w,  list_y,  list_x);
    WINDOW *desc_win  = newwin(box_h,   desc_w,  desc_y,  desc_x);

    int selected  = 0;
    int scroll[3] = {0, 0, 0};
    int ch;

    while (1) {
        draw_border();

        werase(title_win);
        box(title_win, 0, 0);
        wattron(title_win, A_BOLD | A_UNDERLINE);
        mvwprintw(title_win, 1, (title_w - strlen(title_str)) / 2, "%s", title_str);
        wattroff(title_win, A_BOLD | A_UNDERLINE);
        wrefresh(title_win);

        werase(list_win);
        box(list_win, 0, 0);

        for (int i = 0; i < n_loadouts; i++) {
            int name_len = strlen(loadouts[i].name);
            int total_len = 4 + name_len;
            int x = (list_w - total_len) / 2;

            if (i == selected)
                wattron(list_win, A_REVERSE);

            if (i == active_loadout) {
                wattron(list_win, COLOR_PAIR(1) | A_BOLD);
                mvwprintw(list_win, i + 1, x, "[x]");
                wattroff(list_win, COLOR_PAIR(1) | A_BOLD);
            } else {
                mvwprintw(list_win, i + 1, x, "[ ]");
            }

            mvwprintw(list_win, i + 1, x + 4, "%s", loadouts[i].name);

            if (i == selected)
                wattroff(list_win, A_REVERSE);
        }

        mvwhline(list_win, separator_row, 1, ACS_HLINE, list_w - 2);

        if (selected == n_loadouts) {
            wattron(list_win, A_REVERSE);
            mvwprintw(list_win, back_row, (list_w - 4) / 2, "Back");
            wattroff(list_win, A_REVERSE);
        } else {
            mvwprintw(list_win, back_row, (list_w - 4) / 2, "Back");
        }

        wrefresh(list_win);

        werase(desc_win);
        box(desc_win, 0, 0);
        if (selected < n_loadouts) {
            int visible_lines = box_h - 3;
            int total_lines   = wrapped_counts[selected];
            int *s            = &scroll[selected];

            int max_scroll = total_lines - visible_lines;
            if (max_scroll < 0) max_scroll = 0;
            if (*s < 0) *s = 0;
            if (*s > max_scroll) *s = max_scroll;

            wattron(desc_win, A_BOLD);
            mvwprintw(desc_win, 1,
                      (desc_w - strlen(loadouts[selected].name)) / 2,
                      "%s", loadouts[selected].name);
            wattroff(desc_win, A_BOLD);

            for (int i = 0; i < visible_lines && (*s + i) < total_lines; i++) {
                mvwprintw(desc_win, i + 2, 2, "%s", wrapped[selected][*s + i]);
            }
        } else {
            mvwprintw(desc_win, 1, 2, "Return to main menu");
        }
        wrefresh(desc_win);

        ch = getch();
        switch (ch) {
            case KEY_UP:
                selected = (selected - 1 + n_loadouts + 1) % (n_loadouts + 1);
                break;

            case KEY_DOWN:
                selected = (selected + 1) % (n_loadouts + 1);
                break;

            case 10:
                if (selected == n_loadouts) {
                    goto done;
                } else {
                    active_loadout = selected;
                }
                break;
        }
    }

done:
    delwin(title_win);
    delwin(list_win);
    delwin(desc_win);
    clear();
    refresh();
}

void create_credits(void) {
    const char *title_lines[] = { "Credits" };
    int title_h = 3;
    int title_w = 20;

    const char *credit_lines[] = {
        "Development",
        "- Pr1nted",
        "",
        "Original Game",
        "- Mike Klubinka",
        "",
        "Special Thanks",
        "- Developers of ncurses (TUI library)",
        "- You, thanks for playing!"
    };
    int n_credit_lines = sizeof(credit_lines) / sizeof(credit_lines[0]);

    // Auto-calculate credits width
    int credits_w = 0;
    for (int i = 0; i < n_credit_lines; i++) {
        int len = strlen(credit_lines[i]);
        if (len > credits_w) credits_w = len;
    }
    credits_w += 4;
    int credits_h = n_credit_lines + 2;

    int back_w = 10;
    int back_h = 3;

    int total_h = title_h + 1 + credits_h + 1 + back_h;
    int start_y = (LINES - total_h) / 2;

    int title_x   = (COLS - title_w)   / 2;
    int credits_x = (COLS - credits_w) / 2;
    int back_x    = (COLS - back_w)    / 2;

    WINDOW *title_win   = newwin(title_h,   title_w,   start_y,                          title_x);
    WINDOW *credits_win = newwin(credits_h, credits_w, start_y + title_h + 1,            credits_x);
    WINDOW *back_win    = newwin(back_h,    back_w,    start_y + title_h + 1 + credits_h + 1, back_x);

    int ch;
    while (1) {
        draw_border();

        werase(title_win);
        box(title_win, 0, 0);
        wattron(title_win, A_BOLD | A_UNDERLINE);
        mvwprintw(title_win, 1, (title_w - strlen(title_lines[0])) / 2, "%s", title_lines[0]);
        wattroff(title_win, A_BOLD | A_UNDERLINE);
        wrefresh(title_win);

        werase(credits_win);
        box(credits_win, 0, 0);
        for (int i = 0; i < n_credit_lines; i++) {
            if (strlen(credit_lines[i]) > 0 &&
                (i == 0 || credit_lines[i - 1][0] == '\0')) {
                wattron(credits_win, A_BOLD);
                mvwprintw(credits_win, i + 1,
                          (credits_w - strlen(credit_lines[i])) / 2,
                          "%s", credit_lines[i]);
                wattroff(credits_win, A_BOLD);
            } else {
                mvwprintw(credits_win, i + 1,
                          (credits_w - strlen(credit_lines[i])) / 2,
                          "%s", credit_lines[i]);
            }
        }
        wrefresh(credits_win);

        werase(back_win);
        box(back_win, 0, 0);
        wattron(back_win, A_REVERSE);
        mvwprintw(back_win, 1, (back_w - 4) / 2, "Back");
        wattroff(back_win, A_REVERSE);
        wrefresh(back_win);

        ch = getch();
        if (ch == 10) break;
    }

    delwin(title_win);
    delwin(credits_win);
    delwin(back_win);
    clear();
    refresh();
}

void create_lan_menu(void) {
    char title_line[128];
    snprintf(title_line, sizeof(title_line),
             "Play LAN (%s)", loadout_names[active_loadout]);

    const char *title_lines[] = { title_line };
    const char *options[] = {
        "Host LAN game",
        "Join LAN game",
        "Back"
    };

    Menu m = {
        .title_lines   = title_lines,
        .n_title_lines = 1,
        .options       = options,
        .n_options     = 3
    };

    while (1) {
        int choice = create_menu(m);

        if (choice == 0) {
            // Host LAN game
        } else if (choice == 1) {
            // Join LAN game
        } else if (choice == 2) {
            break; // Back
        }
    }
}

// --- Game Screens ---

int show_death_screen(void) {
    int height = 8;
    int width = 24;
    int start_y = (LINES - height) / 2;
    int start_x = (COLS - width) / 2;

    WINDOW *win = newwin(height, width, start_y, start_x);
    keypad(win, TRUE);
    nodelay(win, FALSE);

    const char *options[] = {
        "Try Again",
        "Main Menu"
    };
    int selected = 0;
    int ch;

    while (1) {
        werase(win);
        box(win, 0, 0);

        wattron(win, COLOR_PAIR(1) | A_BOLD);
        mvwprintw(win, 2, (width - 8) / 2, "YOU DIED");
        wattroff(win, COLOR_PAIR(1) | A_BOLD);

        // Options
        for (int i = 0; i < 2; i++) {
            if (i == selected) wattron(win, A_REVERSE);
            mvwprintw(win, 4 + i, (width - strlen(options[i])) / 2, "%s", options[i]);
            if (i == selected) wattroff(win, A_REVERSE);
        }

        wrefresh(win);

        ch = wgetch(win);
        switch (ch) {
            case KEY_UP:
                selected = 0;
                break;
            case KEY_DOWN:
                selected = 1;
                break;
            case 10:
                delwin(win);
                return selected;
            case 'q':
                delwin(win);
                return 1;
        }
    }
}

int show_win_screen(int *current_money, int cigs, int shells, int damage, int beer) {
    int height = 15;
    int width = 40;
    int start_y = (LINES - height) / 2;
    int start_x = (COLS - width) / 2;

    WINDOW *win = newwin(height, width, start_y, start_x);
    keypad(win, TRUE);
    nodelay(win, FALSE);

    const char *options[] = {
        "Double or nothing",
        "Leave"
    };
    int selected = 0;
    int ch;

    while (1) {
        werase(win);
        box(win, 0, 0);

        wattron(win, COLOR_PAIR(4) | A_BOLD);
        mvwprintw(win, 2, (width - 8) / 2, "YOU WIN");
        wattroff(win, COLOR_PAIR(4) | A_BOLD);

        // Statistics
        mvwprintw(win, 4, 2, "Money Earned:   $%d", *current_money);
        mvwprintw(win, 5, 2, "Cigarettes:     %d", cigs);
        mvwprintw(win, 6, 2, "Shells Ejected: %d", shells);
        mvwprintw(win, 7, 2, "Damage Dealt:   %d", damage);
        mvwprintw(win, 8, 2, "Beer Drank:    %d ml", beer);

        mvwhline(win, 10, 1, ACS_HLINE, width - 2);

        for (int i = 0; i < 2; i++) {
            if (i == selected) wattron(win, A_REVERSE);
            mvwprintw(win, 12 + i, (width - strlen(options[i])) / 2, "%s", options[i]);
            if (i == selected) wattroff(win, A_REVERSE);
        }

        wrefresh(win);

        ch = wgetch(win);
        switch (ch) {
            case KEY_UP:
                selected = 0;
                break;
            case KEY_DOWN:
                selected = 1;
                break;
            case 10:
                delwin(win);
                if (selected == 0) {
                    *current_money *= 2;
                    return 0;
                } else {
                    return 1;
                }
            case 'q':
                delwin(win);
                return 1;
        }
    }
}

void draw_main_ui(WINDOW *gwin, GameState *gs) {
    werase(gwin);
    box(gwin, 0, 0);

    int mid_y = LINES / 2;
    int mid_x = COLS / 2;

    // DEALER (top)
    if (gs->cuffs_active && gs->player_turn) {
        wattron(gwin, COLOR_PAIR(1) | A_BOLD);
        mvwprintw(gwin, 1, mid_x - 22, "[HANDCUFFED]");
        wattroff(gwin, COLOR_PAIR(1) | A_BOLD);
    }
    if (!gs->player_turn)
        wattron(gwin, COLOR_PAIR(2) | A_BOLD);
    else
        wattron(gwin, A_BOLD);
    mvwprintw(gwin, 1, mid_x - 8, "DEALER");
    wattroff(gwin, COLOR_PAIR(2) | A_BOLD);
    wattroff(gwin, A_BOLD);
    draw_charges(gwin, 1, mid_x - 1, gs->dealer.charges, gs->dealer.max_charges);

    mvwprintw(gwin, 2, 2, "Dealer items:");

    // CUSTOM HIGHLIGHT LOGIC FOR ADRENALINE
    if (gs->adrenaline_active) {
        for (int i = 0; i < MAX_ITEMS; i++) {
            int x = 2 + i * 12;
            ItemType item = gs->dealer.items[i];

            int is_valid = (item != ITEM_NONE && item != ITEM_ADRENALINE);
            int is_selected = (gs->selected_action == i + 2);

            if (is_selected) {
                wattron(gwin, COLOR_PAIR(3) | A_BOLD | A_REVERSE);
            } else if (is_valid) {
                wattron(gwin, COLOR_PAIR(2) | A_BOLD);
            }

            mvwprintw(gwin, 3, x, "[%-9s]", item_name(item));

            if (is_selected) {
                wattroff(gwin, COLOR_PAIR(3) | A_BOLD | A_REVERSE);
            } else if (is_valid) {
                wattroff(gwin, COLOR_PAIR(2) | A_BOLD);
            }
        }
    } else {
        wattron(gwin, COLOR_PAIR(1));
        draw_items(gwin, 3, 2, gs->dealer.items, -1, 0);
        wattroff(gwin, COLOR_PAIR(1));
    }
    // -------------------------------------------

    mvwhline(gwin, 4, 1, ACS_HLINE, COLS - 2);

    // SHOTGUN (center)
    int gun_y = mid_y - 5;

    if (gs->saw_active) {
        wattron(gwin, A_BOLD | COLOR_PAIR(1));
        mvwprintw(gwin, gun_y, mid_x - 10, "[ SAWN-OFF SHOTGUN ]");
        wattroff(gwin, A_BOLD | COLOR_PAIR(1));
    } else {
        mvwprintw(gwin, gun_y, mid_x - 5, "[ SHOTGUN ]");
    }

    if (gs->show_shells) {
        time_t now = time(NULL);
        if (difftime(now, gs->shell_reveal_time) < 3.0) {
            wattron(gwin, A_BOLD);
            mvwprintw(gwin, gun_y + 1, mid_x - 12,
                      "LIVE: %d  BLANK: %d", gs->live_count, gs->blank_count);
            wattroff(gwin, A_BOLD);
            mvwprintw(gwin, gun_y + 2, mid_x - 11, "memorize shells...");
        } else {
            gs->show_shells = 0;
        }
    } else {
        mvwprintw(gwin, gun_y + 1, mid_x - 8, "LIVE: ?  BLANK: ?");
    }

    if (gs->saw_active) {
        wattron(gwin, A_BOLD | A_REVERSE);
        mvwprintw(gwin, gun_y + 3, mid_x - 6, " SAW ACTIVE ");
        wattroff(gwin, A_BOLD | A_REVERSE);
    }

    // TURN INDICATOR
    wattron(gwin, A_BOLD | A_UNDERLINE | COLOR_PAIR(2));
    if (gs->player_turn)
        mvwprintw(gwin, mid_y - 2, mid_x - 4, "YOUR TURN");
    else
        mvwprintw(gwin, mid_y - 2, mid_x - 6, "DEALER'S TURN");
    wattroff(gwin, A_BOLD | A_UNDERLINE | COLOR_PAIR(2));

    mvwprintw(gwin, mid_y - 1, mid_x - 12,
              "Round %d/%d  |  Sawn off: %s",
              gs->round, gs->roundAmount, gs->saw_active ? "TRUE" : "FALSE");

    // LOG WINDOW
    int log_y = mid_y - 7;
    int log_h = 14;
    WINDOW *log_win = derwin(gwin, log_h, 30, log_y, 2);
    box(log_win, 0, 0);
    mvwprintw(log_win, 0, 2, " LOG ");
    int log_start = (gs->n_log > log_h - 2) ? gs->n_log - (log_h - 2) : 0;
    for (int i = log_start; i < gs->n_log; i++)
        mvwprintw(log_win, i - log_start + 1, 1, "%-28s", gs->log[i]);
    wrefresh(log_win);
    delwin(log_win);

    // ACTIONS WINDOW
    int act_x = COLS - 32;
    WINDOW *act_win = derwin(gwin, 12, 30, mid_y - 4, act_x);
    box(act_win, 0, 0);
    if (gs->adrenaline_active) {
        mvwprintw(act_win, 0, 2, " STEAL ITEM ");
        wattron(act_win, A_BOLD | COLOR_PAIR(1));
        mvwprintw(act_win, 1, 1, " STEALING ITEM...");
        mvwprintw(act_win, 2, 1, " Press 1-8 to pick");
        mvwprintw(act_win, 3, 1, " (Adrenaline excluded)");
        wattroff(act_win, A_BOLD | COLOR_PAIR(1));
    } else if (gs->player_turn) {
        mvwprintw(act_win, 0, 2, " ACTIONS ");
        const char *actions[] = {
            "Shoot dealer [D]",
            "Shoot self   [S]",
            "Use item   [1-8]",
        };
        for (int i = 0; i < 3; i++) {
            if (gs->show_shells) wattron(act_win, COLOR_PAIR(1));
            if (gs->selected_action == i) wattron(act_win, A_REVERSE);
            mvwprintw(act_win, i + 1, 1, " %-28s", actions[i]);
            if (gs->selected_action == i) wattroff(act_win, A_REVERSE);
            if (gs->show_shells) wattroff(act_win, COLOR_PAIR(1));
        }
    } else {
        mvwprintw(act_win, 0, 2, " ACTIONS ");
        mvwprintw(act_win, 1, 1, " The dealer is thinking...");
    }
    wrefresh(act_win);
    delwin(act_win);

    // PLAYER (bottom)
    mvwhline(gwin, LINES - 5, 1, ACS_HLINE, COLS - 2);
    mvwprintw(gwin, LINES - 4, 2,
              gs->adrenaline_active
              ? "Pick from Dealer items above (1-8):"
              : "Your items (Press 1-8 to use):");
    if (gs->show_shells) wattron(gwin, COLOR_PAIR(1));
    draw_items(gwin, LINES - 3, 2, gs->player.items,
               gs->selected_action, (gs->player_turn && !gs->adrenaline_active));
    if (gs->show_shells) wattroff(gwin, COLOR_PAIR(1));

    if (gs->cuffs_active && !gs->player_turn) {
        wattron(gwin, COLOR_PAIR(1) | A_BOLD);
        mvwprintw(gwin, LINES - 2, mid_x - 18, "[HANDCUFFED]");
        wattroff(gwin, COLOR_PAIR(1) | A_BOLD);
    }
    if (gs->player_turn)
        wattron(gwin, COLOR_PAIR(2) | A_BOLD);
    else
        wattron(gwin, A_BOLD);
    mvwprintw(gwin, LINES - 2, mid_x - 4, "YOU");
    wattroff(gwin, COLOR_PAIR(2) | A_BOLD);
    wattroff(gwin, A_BOLD);
    draw_charges(gwin, LINES - 2, mid_x,
                 gs->player.charges, gs->player.max_charges);

    mvwprintw(gwin, LINES - 1, COLS - 8, "[q] exit");

    wrefresh(gwin);
}

int render_game(GameState *gs) {
    WINDOW *gwin = newwin(LINES, COLS, 0, 0);
    box(gwin, 0, 0);
    keypad(gwin, TRUE);
    halfdelay(1);

    int ch;
    while (1) {
        draw_main_ui(gwin, gs);

        // MATCH OVER CHECK
        if (gs->player.charges <= 0 || gs->dealer.charges <= 0) {
            napms(2000);
            cbreak();
            delwin(gwin);
            clear();
            refresh();
            return 1;
        }

        // DEALER LOGIC
        if (!gs->player_turn) {
            dealer_ai_turn(gs, gwin);
            refresh_dealer_view(gs, gwin);
            continue;
        }

        // PLAYER INPUT
        ch = wgetch(gwin);
        if (ch == ERR) continue;
        if (gs->show_shells) {
            gs->show_shells = 0;
            continue;
        }

        switch (ch) {
            case KEY_UP:
                if (gs->adrenaline_active) {
                    gs->selected_action--;
                    if (gs->selected_action < 2) gs->selected_action = 9;
                } else {
                    gs->selected_action = (gs->selected_action > 0) ? gs->selected_action - 1 : 2;
                    if (gs->selected_action > 2) gs->selected_action = 1;
                }
                break;
            case KEY_DOWN:
                if (gs->adrenaline_active) {
                    gs->selected_action++;
                    if (gs->selected_action > 9) gs->selected_action = 2;
                } else {
                    gs->selected_action = (gs->selected_action < 2) ? gs->selected_action + 1 : 0;
                }
                break;
            case KEY_LEFT:
                gs->selected_action--;
                if (gs->adrenaline_active) {
                    if (gs->selected_action < 2) gs->selected_action = 9;
                } else {
                    if (gs->selected_action < 0) gs->selected_action = 9;
                }
                break;
            case KEY_RIGHT:
                gs->selected_action++;
                if (gs->adrenaline_active) {
                    if (gs->selected_action > 9) gs->selected_action = 2;
                } else {
                    if (gs->selected_action > 9) gs->selected_action = 0;
                }
                break;
            case '1': case '2': case '3': case '4':
            case '5': case '6': case '7': case '8':
                gs->selected_action = (ch - '1') + 2;
                break;
            case 10:
                if (gs->adrenaline_active) {
                    int idx = gs->selected_action - 2;
                    if (idx >= 0 && idx < MAX_ITEMS) {
                        ItemType stolen = gs->dealer.items[idx];
                        if (stolen != ITEM_NONE && stolen != ITEM_ADRENALINE) {
                            gs->dealer.items[idx] = ITEM_NONE;
                            for (int k = idx; k < MAX_ITEMS - 1; k++) {
                                gs->dealer.items[k] = gs->dealer.items[k + 1];
                            }
                            gs->dealer.items[MAX_ITEMS - 1] = ITEM_NONE;

                            int added = 0;
                            for (int k = 0; k < MAX_ITEMS; k++) {
                                if (gs->player.items[k] == ITEM_NONE) {
                                    gs->player.items[k] = stolen;
                                    added = 1;
                                    break;
                                }
                            }

                            char msg[64];
                            if (added) {
                                snprintf(msg, sizeof(msg), "You stole %s!", item_name(stolen));
                                game_log(gs, msg);
                            } else {
                                game_log(gs, "Inventory full! Item destroyed.");
                            }

                            gs->adrenaline_active = 0;
                            gs->selected_action = 0;
                        } else {
                            game_log(gs, "Cannot steal that!");
                        }
                    }
                } else {
                    if (gs->selected_action == 0) fire_shotgun(gs, 1, gwin);
                    else if (gs->selected_action == 1) fire_shotgun(gs, 0, gwin);
                    else if (gs->selected_action >= 2) {
                        use_item(gs, gs->selected_action - 2);
                        gs->selected_action = 0;
                    }
                }
                break;
            case 'd': case 'D': fire_shotgun(gs, 1, gwin); break;
            case 's': case 'S': fire_shotgun(gs, 0, gwin); break;
            case 'q': return 0;
        }
    }
}

void create_start_game_menu(void) {
    const char *options[] = {
        "Play offline",
        "Play LAN",
        "Back"
    };

    char title_line[128];
    snprintf(title_line, sizeof(title_line),
             "Start Game (%s)", loadout_names[active_loadout]);

    const char *title_lines[] = { title_line };

    Menu m = {
        .title_lines   = title_lines,
        .n_title_lines = 1,
        .options       = options,
        .n_options     = 3
    };

    while (1) {
        int choice = create_menu(m);

        if (choice == 0) {
            int current_money = 10000; // Starting money

            int running = 1;
            while(running) {
                GameState gs;
                init_game(&gs);

                gs.money = current_money;

                // Round loop
                int playing = 1;
                while (playing && gs.round <= gs.roundAmount) {
                    int result = render_game(&gs);

                    // Player pressed 'q' (Quit to menu)
                    if (result == 0) {
                        playing = 0;
                        running = 0;
                    }
                    // Game Over (someone died)
                    else if (result == 1) {
                        if (gs.player.charges <= 0) {
                            // PLAYER DIED
                            int death_choice = show_death_screen();

                            if (death_choice == 0) {
                                current_money = 10000;
                                init_game(&gs);
                                gs.money = current_money;
                            } else {
                                playing = 0;
                                running = 0;
                            }
                        }
                        else if (gs.dealer.charges <= 0) {
                            if (gs.round > gs.roundAmount) {
                                int win_choice = show_win_screen(&current_money, gs.cigarettes_smoked, gs.shells_ejected, gs.damage_dealt, gs.beer_ml);

                                if (win_choice == 0) {
                                    // Double or nothing - play next round immediately
                                    start_next_round(&gs);
                                } else {
                                    playing = 0;
                                    running = 0;
                                }
                            } else {
                                start_next_round(&gs);
                            }
                        }
                    }
                }
            }

        } else if (choice == 1) {
            create_lan_menu();
        } else if (choice == 2) {
            break;
        }
    }
}