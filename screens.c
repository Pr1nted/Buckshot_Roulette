#include "types.h"
#include "ui.h"
#include "gameplay.h"
#include "screens.h"
#include "network.h"
#ifndef _WIN32
    #include <sys/select.h>
    #include <unistd.h>
#endif

// --- Helper for game loop delay ---
void refresh_dealer_view(GameState *gs, WINDOW *gwin) {
    draw_main_ui(gwin, gs);
    napms(1000);
}

// --- Helper: Error Popup ---
void show_error_popup(const char *message) {
    int h = 8;
    int w = 30;
    int start_y = (LINES - h) / 2;
    int start_x = (COLS - w) / 2;

    WINDOW *win = newwin(h, w, start_y, start_x);
    keypad(win, TRUE);
    nodelay(win, FALSE);

    while(1) {
        werase(win);
        box(win, 0, 0);

        wattron(win, COLOR_PAIR(1) | A_BOLD);
        mvwprintw(win, 2, (w - strlen(message))/2, "%s", message);
        wattroff(win, COLOR_PAIR(1) | A_BOLD);

        mvwprintw(win, 4, (w - 21)/2, "Press Enter to return");

        wrefresh(win);

        int ch = wgetch(win);
        if (ch == 10) break;
    }

    delwin(win);
}

// --- Helper: Host Admin Overlay (Used while Host is Playing) ---
void show_host_console(WINDOW *gwin, GameState *gs) {
    int h = 20;
    int w = 60;
    int start_y = (LINES - h) / 2;
    int start_x = (COLS - w) / 2;

    WINDOW *win = newwin(h, w, start_y, start_x);
    keypad(win, TRUE);
    nodelay(win, FALSE);

    while (1) {
        werase(win);
        box(win, 0, 0);

        // Title
        wattron(win, A_BOLD | COLOR_PAIR(4));
        mvwprintw(win, 1, 2, "HOST ADMIN CONSOLE");
        wattroff(win, A_BOLD | COLOR_PAIR(4));

        // Shell Order
        mvwprintw(win, 3, 2, "Shell Sequence (L=Live, B=Blank):");
        int print_x = 2;
        for (int i = 0; i < gs->n_shells; i++) {
            char shell_char = (gs->shells[i] == SHELL_LIVE) ? 'L' : 'B';

            if (i == gs->current_shell) {
                wattron(win, A_REVERSE | COLOR_PAIR(1));
                mvwprintw(win, 4, print_x, "%c", shell_char);
                wattroff(win, A_REVERSE | COLOR_PAIR(1));
            } else {
                wattron(win, A_DIM);
                if (i > gs->current_shell) wattroff(win, A_DIM);
                mvwprintw(win, 4, print_x, "%c", shell_char);
                wattroff(win, A_DIM);
            }
            print_x += 2;
        }

        // Separator
        mvwhline(win, 6, 1, ACS_HLINE, w - 2);

        // Log History
        mvwprintw(win, 7, 2, "Game Log History:");
        int log_row = 9;
        int max_visible_logs = h - 12;
        int start_log_idx = 0;

        if (gs->n_log > max_visible_logs) {
            start_log_idx = gs->n_log - max_visible_logs;
        }

        for (int i = start_log_idx; i < gs->n_log; i++) {
            mvwprintw(win, log_row++, 2, "%s", gs->log[i]);
        }

        mvwprintw(win, h - 2, w/2 - 10, "Press any key to close");
        wrefresh(win);

        int ch = wgetch(win);
        if (ch != ERR) {
            delwin(win);
            // Restore the main window focus
            touchwin(gwin);
            wrefresh(gwin);
            return;
        }
    }
}

// --- Settings & Menus (Unchanged Logic) ---
void create_settings(void) {
    const char *title_str = "Settings";
    int title_h = 3;
    int title_w = 20;

    const char *options[] = {
        "Flashes",
        "Debug Logs",   // <--- ADD THIS OPTION
        "Back"
    };
    int n_options = sizeof(options) / sizeof(options[0]);

    int list_w = strlen("Debug Logs") + 10; // Ensure width fits new text
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

        // Option 0: Flashes
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

        // Option 1: Debug Logs (NEW)
        if (selected == 1) wattron(list_win, A_REVERSE);
        if (g_debug_mode) {
            wattron(list_win, COLOR_PAIR(2) | A_BOLD);
            mvwprintw(list_win, 2, x, "[x]");
            wattroff(list_win, COLOR_PAIR(2) | A_BOLD);
        } else {
            mvwprintw(list_win, 2, x, "[ ]");
        }
        mvwprintw(list_win, 2, x + 4, "%s", options[1]);
        if (selected == 1) wattroff(list_win, A_REVERSE);

        // Option 2: Back
        if (selected == 2) wattron(list_win, A_REVERSE);
        mvwprintw(list_win, 3, (list_w - strlen(options[2])) / 2, "%s", options[2]);
        if (selected == 2) wattroff(list_win, A_REVERSE);

        wrefresh(list_win);

        ch = getch();
        switch (ch) {
            case KEY_UP:
                selected = (selected - 1 + n_options) % n_options;
                break;
            case KEY_DOWN:
                selected = (selected + 1) % n_options;
                break;
            case 10:
                if (selected == 0) {
                    g_flashes_enabled = !g_flashes_enabled;
                } else if (selected == 1) {
                    g_debug_mode = !g_debug_mode;
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
        "",
        "- Brian Hall for book \"Beej's Guide to Network Programming\"",
        "",
        "- You, thanks for playing!"
    };
    int n_credit_lines = sizeof(credit_lines) / sizeof(credit_lines[0]);

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

// --- LAN Menus & Lobby Logic ---
void run_lobby_screen(NetworkContext *net_ctx) {
    int h = 20;
    int w = 60;
    int start_y = (LINES - h) / 2;
    int start_x = (COLS - w) / 2;

    WINDOW *win = newwin(h, w, start_y, start_x);
    keypad(win, TRUE);
    halfdelay(10);
    nodelay(win, TRUE);

    char local_ip[46] = "Unknown";
    if (net_ctx->is_host) {
        char *result = get_local_ip(local_ip, sizeof(local_ip));
        if (result == NULL) strncpy(local_ip, "Unknown", sizeof(local_ip));
    }

    if (net_ctx->is_host && net_ctx->sock == INVALID_SOCK) {
        delwin(win);
        show_error_popup("Failed to start server (Port in use?)");
        return;
    }

    pthread_mutex_lock(&net_ctx->mutex);
    snprintf(net_ctx->log[0], 64, "Lobby created.");
    net_ctx->log_index = 1;
    pthread_mutex_unlock(&net_ctx->mutex);

    int selected_btn = 0;

    while (1) {
        werase(win);
        box(win, 0, 0);

        pthread_mutex_lock(&net_ctx->mutex);

        wattron(win, A_BOLD);
        mvwprintw(win, 1, 2, "Address: %s", local_ip);
        wattroff(win, A_BOLD);

        mvwprintw(win, 2, 2, "Mode: %s | Loadout: %s | Rounds: %d",
                  net_ctx->is_host_playing ? "Host+Play" : "Host Only",
                  loadout_names[net_ctx->loadout],
                  net_ctx->rounds);

        mvwhline(win, 3, 1, ACS_HLINE, w - 2);

        mvwprintw(win, 4, 2, "Players:");

        int draw_row = 5;

        if (net_ctx->is_host_playing) {
            mvwprintw(win, draw_row, 4, "[Host] %s", net_ctx->host_name);
            draw_row++;
        }

        for (int i = 0; i < net_ctx->client_sockets_count; i++) {
            mvwprintw(win, draw_row, 4, "[P%d] %s", i+1, net_ctx->player_names[i+1]);
            draw_row++;
        }

        mvwhline(win, 10, 1, ACS_HLINE, w - 2);

        mvwprintw(win, 11, 2, "Log:");
        int log_start = (net_ctx->log_index > 5) ? net_ctx->log_index - 5 : 0;
        for (int i = 0; i < 5; i++) {
            int idx = (log_start + i) % MAX_NET_LOG;
            mvwprintw(win, 12 + i, 4, "%s", net_ctx->log[idx]);
        }

        int total_players = (net_ctx->is_host_playing ? 1 : 0) + net_ctx->client_sockets_count;
        int can_start = (total_players >= 2 && total_players <= MAX_PLAYERS);

        pthread_mutex_unlock(&net_ctx->mutex);

        int btn_row = h - 2;

        if (selected_btn == 0) wattron(win, A_REVERSE);
        mvwprintw(win, btn_row, 5, "[Stop Server]");
        if (selected_btn == 0) wattroff(win, A_REVERSE);

        if (selected_btn == 1) {
            if (can_start) wattron(win, A_REVERSE);
            else wattron(win, A_DIM);
        } else {
            if (!can_start) wattron(win, A_DIM);
        }

        mvwprintw(win, btn_row, w - 20, "[Start Game]");
        wattroff(win, A_REVERSE);
        wattroff(win, A_DIM);

        wrefresh(win);

        int ch = wgetch(win);
        if (ch == ERR) continue;

        switch (ch) {
            case KEY_LEFT:
            case KEY_RIGHT:
                selected_btn = (selected_btn + 1) % 2;
                break;
            case 10:
                if (selected_btn == 0) {
                    network_disconnect(net_ctx);
                    goto done;
                } else if (selected_btn == 1 && can_start) {
                    mvwprintw(win, 10, w/2 - 6, "STARTING GAME...");
                    wrefresh(win);
                    napms(2000);
                    goto done;
                }
                break;
            case 27:
                network_disconnect(net_ctx);
                goto done;
        }
    }

done:
    nodelay(win, FALSE);
    delwin(win);
}

void run_client_lobby_screen(NetworkContext *net_ctx, const char *client_name) {
    // Poll for up to 10 seconds; if still not connected, show error and bail.
    {
        int waited_ms = 0;
        int max_wait_ms = 10000; // Increased to 10 seconds for network latency

        while (waited_ms < max_wait_ms) {
            pthread_mutex_lock(&net_ctx->mutex);
            bool connected = net_ctx->is_connected;
            bool sock_ok   = (net_ctx->sock != INVALID_SOCK);
            bool has_error  = !connected && sock_ok; // Thread might have set error
            pthread_mutex_unlock(&net_ctx->mutex);

            if (connected && sock_ok) break;

            // Check if thread has already failed definitively
            if (!connected && !sock_ok) {
                show_error_popup("Could not connect to server! (Connection refused or timed out)");
                return;
            }

#ifdef _WIN32
            Sleep(100);  // Windows - 100ms intervals
#else
            usleep(100000); // POSIX - 100ms intervals
#endif
            waited_ms += 100;
        }

        pthread_mutex_lock(&net_ctx->mutex);
        bool connected = net_ctx->is_connected;
        bool sock_ok   = (net_ctx->sock != INVALID_SOCK);
        pthread_mutex_unlock(&net_ctx->mutex);

        if (!connected || !sock_ok) {
            if (strcmp(net_ctx->remote_ip, "127.0.0.1") == 0 ||
                strlen(net_ctx->remote_ip) == 0) {
                show_error_popup("Invalid server IP address!");
            } else {
                show_error_popup("Could not connect to server! (Timeout)");
            }
            return;
        }
    }

    // --- Wrap entire screen in while loop ---
    while (!net_ctx->should_stop) {
        int h = 20;
        int w = 60;
        int start_y = (LINES - h) / 2;
        int start_x = (COLS - w) / 2;

        WINDOW *win = newwin(h, w, start_y, start_x);
        keypad(win, TRUE);
        halfdelay(10);
        nodelay(win, TRUE);

        pthread_mutex_lock(&net_ctx->mutex);
        snprintf(net_ctx->log[0], 64, "Connected to Host.");
        net_ctx->log_index = 1;
        pthread_mutex_unlock(&net_ctx->mutex);

        while (1) {
            pthread_mutex_lock(&net_ctx->mutex);
            bool started = net_ctx->game_started;
            bool connected = net_ctx->is_connected;
            int sock_valid = (net_ctx->sock != INVALID_SOCK);
            pthread_mutex_unlock(&net_ctx->mutex);

            if ((!sock_valid && !connected) || net_ctx->should_stop) {
                delwin(win);
                show_error_popup("Connection lost or disconnected!");
                return;
            }

            if (started) {
                break; // Break the drawing loop to start the game
            }

            werase(win);
            box(win, 0, 0);

            pthread_mutex_lock(&net_ctx->mutex);
            mvwprintw(win, 1, 2, "Host: %s | Loadout: %s",
                      net_ctx->remote_ip,
                      (net_ctx->loadout >= 0 && net_ctx->loadout < 3) ? loadout_names[net_ctx->loadout] : "Unknown");
            mvwprintw(win, 2, 22, "Rounds: %d", net_ctx->rounds);
            mvwhline(win, 3, 1, ACS_HLINE, w - 2);
            mvwprintw(win, 4, 2, "Players:");
            int draw_row = 5;
            if (net_ctx->is_host_playing) {
                mvwprintw(win, draw_row, 4, "[Host] %s", net_ctx->player_names[0]);
                draw_row++;
            }
            int start_index = net_ctx->is_host_playing ? 1 : 0;
            for (int i = start_index; i < MAX_PLAYERS; i++) {
                if (net_ctx->player_names[i][0] != '\0') {
                    if (strcmp(net_ctx->player_names[i], client_name) == 0) {
                         mvwprintw(win, draw_row, 4, "[You] %s", net_ctx->player_names[i]);
                    } else {
                         mvwprintw(win, draw_row, 4, "[P%d] %s", i+1, net_ctx->player_names[i]);
                    }
                    draw_row++;
                }
            }
            mvwhline(win, 10, 1, ACS_HLINE, w - 2);
            mvwprintw(win, 11, 2, "Log:");
            int log_start = (net_ctx->log_index > 5) ? net_ctx->log_index - 5 : 0;
            for (int i = 0; i < 5; i++) {
                int idx = (log_start + i) % MAX_NET_LOG;
                mvwprintw(win, 12 + i, 4, "%s", net_ctx->log[idx]);
            }
            pthread_mutex_unlock(&net_ctx->mutex);

            int btn_row = h - 2;
            wattron(win, A_REVERSE);
            mvwprintw(win, btn_row, w/2 - 8, "[Disconnect]");
            wattroff(win, A_REVERSE);
            wrefresh(win);

            int ch = wgetch(win);
            if (ch == ERR) continue;
            switch (ch) {
                case 10:
                case 27:
                    network_disconnect(net_ctx);
                    return; // Exit completely
            }
        }

        // --- START THE MATCH ---
        cbreak();
        delwin(win);
        clear();
        refresh();

        GameState gs;
        init_lan_game(&gs, net_ctx);
        render_multiplayer_game(&gs, net_ctx);

        // Match ended. Reset local flag so the lobby redraws.
        pthread_mutex_lock(&net_ctx->mutex);
        net_ctx->game_started = false;
        pthread_mutex_unlock(&net_ctx->mutex);
    }
}

void create_lan_host_menu(void) {
    const char *title_str = "Host LAN Game";
    int title_h = 3;
    int title_w = 20;

    int selected_rounds = 3;
    int temp_loadout = active_loadout;
    int is_host_only = 1;

    char username_buffer[32] = "Host";
    int user_len = strlen(username_buffer);

    char rounds_opt[32];
    char loadout_opt[64];
    char name_opt[64];

    const char *options[] = {
        rounds_opt,
        loadout_opt,
        name_opt,
        "Start Hosting",
        "Back"
    };
    int n_options = sizeof(options) / sizeof(options[0]);

    int list_w = 35;
    int list_h = 9;

    int title_x = (COLS - title_w) / 2;
    int title_y = (LINES / 2) - (title_h + list_h) / 2;
    int list_x  = (COLS - list_w) / 2;
    int list_y  = title_y + title_h + 1;

    WINDOW *title_win = newwin(title_h, title_w, title_y, title_x);
    WINDOW *list_win  = newwin(list_h,  list_w,  list_y,  list_x);

    int selected = 0;
    int ch;

    static int net_inited = 0;
    if (!net_inited) {
        network_init();
        net_inited = 1;
    }

    while (1) {
        draw_border();

        snprintf(rounds_opt, sizeof(rounds_opt), "Rounds: %d", selected_rounds);
        snprintf(loadout_opt, sizeof(loadout_opt), "Loadout: %s", loadout_names[temp_loadout]);
        snprintf(name_opt, sizeof(name_opt), "Name: %s", username_buffer);

        werase(title_win);
        box(title_win, 0, 0);
        wattron(title_win, A_BOLD | A_UNDERLINE);
        mvwprintw(title_win, 1, (title_w - strlen(title_str)) / 2, "%s", title_str);
        wattroff(title_win, A_BOLD | A_UNDERLINE);
        wrefresh(title_win);

        werase(list_win);
        box(list_win, 0, 0);

        int current_row = 1;

        for (int i = 0; i < n_options; i++) {
            int x = (list_w - strlen(options[i])) / 2;
            int dim_name = (i == 2 && is_host_only);
            int dim_toggle = 0;

            if (i == selected) {
                if (dim_name) wattron(list_win, A_DIM | A_REVERSE);
                else wattron(list_win, A_REVERSE);
            } else if (dim_name) {
                wattron(list_win, A_DIM);
            }

            if ((i == 0 || i == 1 || i == 2) && i == selected) {
                mvwprintw(list_win, current_row, x - 2, "<");
                mvwprintw(list_win, current_row, x, "%s", options[i]);
                mvwprintw(list_win, current_row, x + strlen(options[i]) + 1, ">");
            } else {
                mvwprintw(list_win, current_row, x, "%s", options[i]);
            }

            wattroff(list_win, A_REVERSE);
            wattroff(list_win, A_DIM);
            current_row++;

            if (i == 0 && i == selected) {
                mvwprintw(list_win, current_row, (list_w - 18)/2, "<- Rounds Change ->");
                current_row++;
            }
            if (i == 1 && i == selected) {
                mvwprintw(list_win, current_row, (list_w - 22)/2, "<- Loadout Change ->");
                current_row++;
            }
            if (i == 2 && i == selected && !is_host_only) {
                 mvwprintw(list_win, current_row, (list_w - 20)/2, "(Type Username)");
                 current_row++;
            }
        }
        wrefresh(list_win);

        ch = getch();
        switch (ch) {
            case KEY_UP:
                selected = (selected - 1 + n_options) % n_options;
                break;
            case KEY_DOWN:
                selected = (selected + 1) % n_options;
                break;

            case KEY_LEFT:
                if (selected == 0) {
                    selected_rounds--;
                    if (selected_rounds < 1) selected_rounds = 1;
                } else if (selected == 1) {
                    temp_loadout--;
                    if (temp_loadout < 0) temp_loadout = 2;
                    active_loadout = temp_loadout;
                }
                break;

            case KEY_RIGHT:
                if (selected == 0) {
                    selected_rounds++;
                    if (selected_rounds > 10) selected_rounds = 10;
                } else if (selected == 1) {
                    temp_loadout++;
                    if (temp_loadout > 2) temp_loadout = 0;
                    active_loadout = temp_loadout;
                }
                break;

            case 10:
                if (selected == 3) {
                    NetworkContext net_ctx;
                    if (network_start_server(&net_ctx) == 0) {
                        net_ctx.rounds = selected_rounds;
                        net_ctx.loadout = active_loadout;
                        net_ctx.is_host_playing = !is_host_only;
                        strncpy(net_ctx.host_name, username_buffer, sizeof(net_ctx.host_name));

                        // --- LOOP LOBBY AND GAME ---
                        while (!net_ctx.should_stop) {
                            run_lobby_screen(&net_ctx);

                            if (net_ctx.should_stop) break;

                            network_broadcast_start(&net_ctx);

                            GameState gs;
                            init_lan_game(&gs, &net_ctx);

                            if (net_ctx.is_host_playing) {
                                render_multiplayer_game(&gs, &net_ctx);
                            } else {
                                render_host_admin_console(&gs, &net_ctx);
                            }

                            // Match Over! Reset state to return to lobby
                            pthread_mutex_lock(&net_ctx.mutex);
                            net_ctx.game_started = false;
                            pthread_mutex_unlock(&net_ctx.mutex);
                        }

                        network_disconnect(&net_ctx);
                    }
                } else if (selected == 4) {
                    goto done;
                }
                break;
        }

        if (selected == 2 && !is_host_only) {
            if (ch == KEY_BACKSPACE || ch == 127) {
                if (user_len > 0) {
                    username_buffer[--user_len] = '\0';
                }
            } else if (ch >= ' ' && ch <= '~') {
                if (user_len < 31) {
                    username_buffer[user_len++] = ch;
                    username_buffer[user_len] = '\0';
                }
            }
        }
    }

done:
    delwin(title_win);
    delwin(list_win);
    clear();
    refresh();
}

void create_lan_join_menu(void) {
    const char *title_str = "Join LAN Game";
    int title_h = 3;
    int title_w = 20;

    char ip_buffer[46] = "127.0.0.1";
    int ip_len = strlen(ip_buffer);

    char username_buffer[32] = "Player";
    int user_len = strlen(username_buffer);

    char connect_btn[64];
    char name_btn[64];

    const char *options[] = {
        connect_btn,
        name_btn,
        "Back"
    };
    int n_options = sizeof(options) / sizeof(options[0]);

    int list_w = 35;
    int list_h = 7;

    int title_x = (COLS - title_w) / 2;
    int title_y = (LINES / 2) - (title_h + list_h) / 2;
    int list_x  = (COLS - list_w) / 2;
    int list_y  = title_y + title_h + 1;

    WINDOW *title_win = newwin(title_h, title_w, title_y, title_x);
    WINDOW *list_win  = newwin(list_h,  list_w,  list_y,  list_x);

    int selected = 0;
    int ch;

    static int net_inited = 0;
    if (!net_inited) {
        network_init();
        net_inited = 1;
    }

    while (1) {
        draw_border();

        snprintf(connect_btn, sizeof(connect_btn), "Connect: %s", ip_buffer);
        snprintf(name_btn, sizeof(name_btn), "Name: %s", username_buffer);

        werase(title_win);
        box(title_win, 0, 0);
        wattron(title_win, A_BOLD | A_UNDERLINE);
        mvwprintw(title_win, 1, (title_w - strlen(title_str)) / 2, "%s", title_str);
        wattroff(title_win, A_BOLD | A_UNDERLINE);
        wrefresh(title_win);

        werase(list_win);
        box(list_win, 0, 0);

        int current_row = 1;

        for (int i = 0; i < n_options; i++) {
            int x = (list_w - strlen(options[i])) / 2;
            if (i == selected) wattron(list_win, A_REVERSE);
            mvwprintw(list_win, current_row, x, "%s", options[i]);
            if (i == selected) wattroff(list_win, A_REVERSE);
            current_row++;

            if (i == 0 && i == selected) {
                mvwprintw(list_win, current_row, (list_w - 20)/2, "(Type to edit IP)");
                current_row++;
            }
            if (i == 1 && i == selected) {
                mvwprintw(list_win, current_row, (list_w - 20)/2, "(Type Username)");
                current_row++;
            }
        }
        wrefresh(list_win);

        ch = getch();

        if (ch == KEY_UP) {
            selected = (selected - 1 + n_options) % n_options;
        }
        else if (ch == KEY_DOWN) {
            selected = (selected + 1) % n_options;
        }
        else if (ch == 10) {
            if (selected == 0) {
                NetworkContext net_ctx;
                if (network_start_client(&net_ctx, ip_buffer, username_buffer) == 0) {
                    run_client_lobby_screen(&net_ctx, username_buffer);
                    network_disconnect(&net_ctx);
                }
            } else if (selected == 2) {
                goto done;
            }
        }
        else if (selected == 0) {
            if (ch == KEY_BACKSPACE || ch == 127) {
                if (ip_len > 0) ip_buffer[--ip_len] = '\0';
            } else if (ch >= ' ' && ch <= '~') {
                if ((ch >= '0' && ch <= '9') || ch == '.') {
                    if (ip_len < 45) { ip_buffer[ip_len++] = ch; ip_buffer[ip_len] = '\0'; }
                }
            }
        }
        else if (selected == 1) {
            if (ch == KEY_BACKSPACE || ch == 127) {
                if (user_len > 0) username_buffer[--user_len] = '\0';
            } else if (ch >= ' ' && ch <= '~') {
                if (user_len < 31) { username_buffer[user_len++] = ch; username_buffer[user_len] = '\0'; }
            }
        }
    }

done:
    delwin(title_win);
    delwin(list_win);
    clear();
    refresh();
}

void create_lan_menu(void) {
    char title_line[128];
    snprintf(title_line, sizeof(title_line),
             "Play LAN (%s)", loadout_names[active_loadout]);

    const char *title_lines[] = { title_line };
    const char *options[] = {
        "Host game",
        "Join game",
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
            create_lan_host_menu();
        } else if (choice == 1) {
            create_lan_join_menu();
        } else if (choice == 2) {
            break;
        }
    }
}

// --- Game Logic Helpers ---

void init_lan_game(GameState *gs, NetworkContext *net_ctx) {
    memset(gs, 0, sizeof(GameState));

    char header;
    SyncPacket sp;
    int sync_received = 0;

    gs->active_loadout = net_ctx->loadout;
    gs->roundAmount = net_ctx->rounds;
    gs->round = 1;
    gs->saw_active = 0;
    gs->cuffs_target = -1;

    if (net_ctx->is_host) {
        gs->is_host = 1;
        pthread_mutex_lock(&net_ctx->mutex);

        gs->player_count = net_ctx->player_count;
        gs->current_player_index = 0;

        int round_max_charges = RangeRand(2, 4);

        int player_slot = 0;
        for(int i=0; i<MAX_PLAYERS && player_slot < gs->player_count; i++) {
            if(net_ctx->player_names[i][0] == '\0') continue;
            strncpy(gs->players[player_slot].name, net_ctx->player_names[i], 32);
            gs->players[player_slot].max_charges = round_max_charges;
            gs->players[player_slot].charges = round_max_charges;
            for(int k=0; k<MAX_ITEMS; k++) {
                gs->players[player_slot].items[k] = ITEM_NONE;
            }
            player_slot++;
        }
        pthread_mutex_unlock(&net_ctx->mutex);

        setup_player_positions(gs);
        generate_shells(gs);
        give_new_items(gs);

        sp.n_shells = gs->n_shells;
        sp.live_count = gs->live_count;
        sp.blank_count = gs->blank_count;
        sp.current_shell = gs->current_shell;
        sp.round = gs->round;

        memcpy(sp.players, gs->players, sizeof(gs->players));
        memcpy(sp.visual_map, gs->visual_map, sizeof(gs->visual_map));

        send_sync(net_ctx, gs);
        sync_received = 1;

        gs->local_player_index = 0;

        gs->show_shells = 1;
        gs->shell_reveal_time = time(NULL);
        game_log(gs, "Game Started. Synced.");

        if (g_debug_mode) {
            #ifdef _WIN32
                game_log(gs, "[DBG] OS: Windows (WinSock2)");
            #elif defined(__APPLE__)
                game_log(gs, "[DBG] OS: macOS (BSD sockets)");
            #elif defined(__linux__)
                game_log(gs, "[DBG] OS: Linux (POSIX sockets)");
            #else
                game_log(gs, "[DBG] OS: Unknown POSIX");
            #endif

            char dbuf[64];
            snprintf(dbuf, 64, "[DBG] Host. Clients: %d  Players: %d",
                     net_ctx->client_sockets_count, gs->player_count);
            game_log(gs, dbuf);

            snprintf(dbuf, 64, "[DBG] SyncPkt: %d B  ActionPkt: %d B",
                     (int)sizeof(SyncPacket), (int)sizeof(ActionPacket));
            game_log(gs, dbuf);
        }
    } else {
        game_log(gs, "Waiting for Host sync...");

        if (g_debug_mode) {
            #ifdef _WIN32
                game_log(gs, "[DBG] OS: Windows (WinSock2)");
            #elif defined(__APPLE__)
                game_log(gs, "[DBG] OS: macOS (BSD sockets)");
            #elif defined(__linux__)
                game_log(gs, "[DBG] OS: Linux (POSIX sockets)");
            #else
                game_log(gs, "[DBG] OS: Unknown POSIX");
            #endif
            char dbuf[64];
            snprintf(dbuf, 64, "[DBG] Client idx %d  Players: %d",
                     gs->local_player_index, gs->player_count);
            game_log(gs, dbuf);
        }

        if (recv_packet(net_ctx->sock, &header, &sp, sizeof(sp), gs) != 0 || header != PKT_SYNC) {
            game_log(gs, "Error: Failed to sync with host.");
            return;
        }

        gs->n_shells = sp.n_shells;
        gs->live_count = sp.live_count;
        gs->blank_count = sp.blank_count;
        gs->current_shell = sp.current_shell;
        gs->round = sp.round;

        for(int i=0; i<MAX_SHELLS; i++) {
            gs->shells[i] = SHELL_UNKNOWN;
        }

        memcpy(gs->players, sp.players, sizeof(gs->players));
        memcpy(gs->visual_map, sp.visual_map, sizeof(gs->visual_map));

        gs->player_count = 0;
        for (int i = 0; i < MAX_PLAYERS; i++) {
            if (sp.players[i].name[0] != '\0') {
                gs->player_count++;
            } else {
                break;
            }
        }

        if (gs->player_count == 0 && net_ctx->player_count > 0) {
             game_log(gs, "Warning: Sync packet corrupted. Using Lobby count.");
             gs->player_count = net_ctx->player_count;
        }

        sync_received = 1;
        gs->show_shells = 1;
        gs->shell_reveal_time = time(NULL);

        char my_name[32];
        strncpy(my_name, net_ctx->host_name, 32);

        gs->local_player_index = -1;
        for (int i = 0; i < gs->player_count; i++) {
            if (strcmp(gs->players[i].name, my_name) == 0) {
                gs->local_player_index = i;
                break;
            }
        }

        if (gs->local_player_index == -1) {
             game_log(gs, "Error: Could not find own player index.");
             gs->local_player_index = 0;
        }

        if (gs->player_count > 0) {
            game_log(gs, "Sync Received. Game Start!");
        } else {
            game_log(gs, "Fatal Error: No players detected. Returning to menu.");
            return;
        }
    }
}

// --- Helper to apply a packet to the local game state ---
void apply_action(GameState *gs, ActionPacket *ap, WINDOW *gwin) {
    gs->current_player_index = ap->actor_idx;
    gs->player_turn = (ap->actor_idx == gs->local_player_index);

    if (ap->action_type == 0) {
        perform_multiplayer_shoot(gs, ap->actor_idx, ap->target_idx, gwin);

        if (gs->current_shell >= gs->n_shells) {
            generate_shells(gs);
            give_new_items(gs);
            char reload_msg[64];
            snprintf(reload_msg, sizeof(reload_msg),
                     "Reloaded: %d live, %d blank.", gs->live_count, gs->blank_count);
            game_log(gs, reload_msg);
            gs->show_shells = 1;
            gs->shell_reveal_time = time(NULL);
        }
    }
    else if (ap->action_type == 1) {
        // USE ITEM
        use_item(gs, ap->item_idx, ap->actor_idx, ap->target_idx);
    }
    else if (ap->action_type == 2) {
        // STEAL ITEM
        perform_steal(gs, ap->actor_idx, ap->target_idx, ap->item_idx);
    }
}

void apply_sync_packet(GameState *gs, SyncPacket *sp) {
    gs->n_shells    = sp->n_shells;
    gs->live_count  = sp->live_count;
    gs->blank_count = sp->blank_count;
    gs->current_shell = sp->current_shell;
    gs->round       = sp->round;
    gs->current_player_index = sp->current_player_index;
    gs->cuffs_target = sp->cuffs_target;

    // Trigger shell reveal animation if host flagged it (e.g. after reload)
    if (sp->show_shells) {
        gs->show_shells = 1;
        gs->shell_reveal_time = time(NULL);
    }

    memcpy(gs->shells, sp->shells, sizeof(gs->shells));

    for (int i = 0; i < gs->player_count; i++) {
        gs->players[i] = sp->players[i];
    }

    gs->awaiting_sync = 0;
}

// --- Render Multiplayer Game ---
void render_multiplayer_game(GameState *gs, NetworkContext *net_ctx) {
    WINDOW *gwin = newwin(LINES, COLS, 0, 0);
    box(gwin, 0, 0);
    keypad(gwin, TRUE);
    halfdelay(5);

    fd_set readfds;
    struct timeval tv;
    int max_sd;

    while (1) {
        draw_main_ui(gwin, gs);

        // --- GAME OVER / ROUND END CHECK ---
        int alive_count = 0;
        int my_dead = (gs->players[gs->local_player_index].charges <= 0);

        for(int i=0; i<gs->player_count; i++) {
            if(gs->players[i].charges > 0) alive_count++;
        }

        if (alive_count <= 1) {
            napms(2000);

            // Identify the winner
            int winner_idx = -1;
            for(int i = 0; i < gs->player_count; i++) {
                if(gs->players[i].charges > 0) {
                    winner_idx = i;
                    break;
                }
            }

            if (gs->round >= gs->roundAmount) {
                // Prepare the victory message
                char winner_msg[64];
                if (winner_idx != -1) {
                    snprintf(winner_msg, 64, "MATCH OVER: %s is the Victor!", gs->players[winner_idx].name);
                } else {
                    snprintf(winner_msg, 64, "MATCH OVER: No survivors.");
                }

                game_log(gs, winner_msg);
                if (net_ctx->is_host) {
                    broadcast_log(net_ctx, winner_msg);

                    pthread_mutex_lock(&net_ctx->mutex);
                    strncpy(net_ctx->log[net_ctx->log_index], winner_msg, 64);
                    net_ctx->log_index = (net_ctx->log_index + 1) % MAX_NET_LOG;
                    pthread_mutex_unlock(&net_ctx->mutex);
                }

                mvwprintw(gwin, LINES/2, COLS/2 - 15, "GAME OVER - RETURNING TO LOBBY");
                wrefresh(gwin);
                napms(3000);

                delwin(gwin);
                return; // Return to the lobby loop
            } else {
                // Normal round transition logic...
                if (net_ctx->is_host) {
                    start_next_round(gs);
                    send_sync(net_ctx, gs);
                }
            }
        }

        // --- SELECT SETUP ---
        FD_ZERO(&readfds);
        max_sd = 0;

        if (net_ctx->is_host) {
            pthread_mutex_lock(&net_ctx->mutex);
            for (int i = 0; i < net_ctx->client_sockets_count; i++) {
                socket_t sd = net_ctx->clients[i];
                if (sd != INVALID_SOCK) {
                    FD_SET(sd, &readfds);
                    if ((int)sd > max_sd) max_sd = (int)sd;
                }
            }
            pthread_mutex_unlock(&net_ctx->mutex);
        } else {
            if (net_ctx->sock != INVALID_SOCK) {
                FD_SET(net_ctx->sock, &readfds);
                if ((int)net_ctx->sock > max_sd) max_sd = (int)net_ctx->sock;
            }
        }

        tv.tv_sec = 0;
        tv.tv_usec = 100000; // 100ms timeout

        int activity = 0;
        if (max_sd > 0) {
            activity = select(max_sd + 1, &readfds, NULL, NULL, &tv);
        }

        if (activity > 0) {
            // --- HOST NETWORKING ---
            if (net_ctx->is_host) {
                pthread_mutex_lock(&net_ctx->mutex);
                for (int i = 0; i < net_ctx->client_sockets_count; i++) {
                    int sd = net_ctx->clients[i];
                    if (FD_ISSET(sd, &readfds)) {
                        char header;
                        ActionPacket ap;

                        int res = recv_packet(sd, &header, &ap, sizeof(ap), gs);

                        if (res == 0) {
                            if (header == PKT_ACTION) {
                                int log_before = gs->n_log;
                                apply_action(gs, &ap, gwin);

                                pthread_mutex_unlock(&net_ctx->mutex);
                                for (int l = log_before; l < gs->n_log; l++) {
                                    if (strncmp(gs->log[l], "[PRIVATE] ", 10) == 0) {
                                        const char *reveal = gs->log[l] + 10;
                                        memmove(gs->log[l], reveal, strlen(reveal) + 1);
                                        send_log_to_player(net_ctx, ap.actor_idx, gs->log[l]);
                                    } else {
                                        broadcast_log(net_ctx, gs->log[l]);
                                    }
                                }

                                broadcast_action(net_ctx, &ap);
                                send_sync(net_ctx, gs);

                                goto SKIP_HOST_UNLOCK;
                            } else {
                                char msg[64];
                                snprintf(msg, 64, "Unknown packet: %c", header);
                                game_log(gs, msg);
                            }
                        } else {
                            game_log(gs, "A player disconnected.");
                        }
                    }
                }
                SKIP_HOST_UNLOCK:;
                pthread_mutex_unlock(&net_ctx->mutex);
            }
            // --- CLIENT NETWORKING ---
            else {
                if (FD_ISSET(net_ctx->sock, &readfds)) {
                    char header;
                    int r = recv(net_ctx->sock, &header, 1, 0);

                    if (r == 0) {
                        // Connection closed gracefully
                        CLOSE_SOCKET(net_ctx->sock);
                        net_ctx->sock = INVALID_SOCK;
                        net_ctx->is_connected = false;
                        break;
                    }

                    if (r < 0) {
                        int err = GET_SOCKET_ERROR();
                        if (err == EAGAIN || err == EWOULDBLOCK) continue;

                        CLOSE_SOCKET(net_ctx->sock);
                        net_ctx->sock = INVALID_SOCK;
                        net_ctx->is_connected = false;
                        break;
                    }

                    if (header == PKT_SYNC) {
                        SyncPacket sp;
                        if (recv_body(net_ctx->sock, &sp, sizeof(sp)) != 0) {
                            game_log(gs, "Disconnected (Sync).");
                            goto done;
                        }
                        apply_sync_packet(gs, &sp);
                    }
                    else if (header == PKT_LOG) {
                        LogPacket lp;
                        if (recv_body(net_ctx->sock, &lp, sizeof(lp)) != 0) {
                            game_log(gs, "Disconnected (Log).");
                            goto done;
                        }
                        game_log(gs, lp.msg);
                    }
                    else if (header == PKT_ACTION) {
                        ActionPacket ap;
                        if (recv_body(net_ctx->sock, &ap, sizeof(ap)) != 0) {
                            game_log(gs, "Disconnected (Action).");
                            goto done;
                        }
                        apply_action(gs, &ap, gwin);
                    }
                }
            }
        }

        // --- INPUT HANDLING ---
        int ch = wgetch(gwin);
        if (ch == ERR) continue;

        if (ch == 'q') break;

        if (net_ctx->is_host && ch == 'h') {
            show_host_console(gwin, gs);
            continue;
        }

        if (gs->current_player_index == gs->local_player_index && !gs->awaiting_sync && !my_dead) {
            if ((gs->adrenaline_active || gs->adrenaline_steal_phase) && ch == 27) {
                gs->adrenaline_active = 0;
                gs->adrenaline_steal_phase = 0;
                gs->adrenaline_steal_slot = 0;
                gs->steal_target_idx = -1;
                game_log(gs, "Adrenaline Cancelled.");
                continue;
            }
            if (gs->handcuffs_selecting && ch == 27) {
                gs->handcuffs_selecting = 0;
                gs->steal_target_idx = -1;
                game_log(gs, "Handcuffs Cancelled.");
                continue;
            }

            int num_oponents = gs->player_count - 1;
            int total_options = num_oponents + 2;
            int shoot_options = num_oponents + 1;

            ActionPacket ap;
            ap.actor_idx = gs->local_player_index;

            switch (ch) {
                case KEY_UP:
                case KEY_DOWN:
                    if (gs->adrenaline_steal_phase) {
                        int dir = (ch == KEY_DOWN) ? 1 : -1;
                        gs->adrenaline_steal_slot = (gs->adrenaline_steal_slot + dir + MAX_ITEMS) % MAX_ITEMS;
                    } else if (gs->adrenaline_active || gs->handcuffs_selecting) {
                        int current_opp_idx = -1;
                        int opp_count = 0;

                        for (int i = 0; i < gs->player_count; i++) {
                            if (i != gs->local_player_index) {
                                if (i == gs->steal_target_idx) current_opp_idx = opp_count;
                                opp_count++;
                            }
                        }

                        int new_dir = (ch == KEY_DOWN) ? 1 : -1;
                        int new_opp_idx = current_opp_idx + new_dir;
                        if (new_opp_idx < 0) new_opp_idx = opp_count - 1;
                        if (new_opp_idx >= opp_count) new_opp_idx = 0;

                        opp_count = 0;
                        for (int i = 0; i < gs->player_count; i++) {
                            if (i != gs->local_player_index) {
                                if (opp_count == new_opp_idx) {
                                    gs->steal_target_idx = i;
                                    break;
                                }
                                opp_count++;
                            }
                        }
                    } else {
                        if (ch == KEY_UP) {
                            gs->selected_action--;
                            if (gs->selected_action < 0) gs->selected_action = total_options - 1;
                        } else {
                            gs->selected_action++;
                            if (gs->selected_action >= total_options) gs->selected_action = 0;
                        }
                    }
                    break;

                case '1': case '2': case '3': case '4':
                case '5': case '6': case '7': case '8':
                    if (gs->adrenaline_steal_phase) {
                        gs->adrenaline_steal_slot = (ch - '1');
                    } else if (!gs->adrenaline_active && !gs->handcuffs_selecting) {
                        gs->selected_item = (ch - '1');
                    }
                    break;

                case 10:
                    // --- HANDCUFFS CONFIRM ---
                    if (gs->handcuffs_selecting) {
                        int target = gs->steal_target_idx;

                        int slot = -1;
                        Player *p = &gs->players[gs->local_player_index];
                        for(int k=0; k<MAX_ITEMS; k++) {
                            if(p->items[k]==ITEM_HANDCUFFS) { slot=k; break; }
                        }

                        if(slot != -1 && target != -1) {
                            ap.action_type = 1;
                            ap.item_idx = slot;
                            ap.target_idx = target;
                            ap.actor_idx = gs->local_player_index;

                            if (g_debug_mode) game_log(gs, "[DEBUG] Confirming Handcuffs.");

                            if (net_ctx->is_host) {
                                int log_before = gs->n_log;
                                apply_action(gs, &ap, gwin);
                                for (int l = log_before; l < gs->n_log; l++) {
                                    if (strncmp(gs->log[l], "[PRIVATE] ", 10) == 0) {
                                        memmove(gs->log[l], gs->log[l] + 10, strlen(gs->log[l] + 10) + 1);
                                        send_log_to_player(net_ctx, ap.actor_idx, gs->log[l]);
                                    } else {
                                        broadcast_log(net_ctx, gs->log[l]);
                                    }
                                }
                                broadcast_action(net_ctx, &ap);
                                send_sync(net_ctx, gs);
                            } else {
                                send_action_to_host(net_ctx, gs, &ap);
                                gs->awaiting_sync = 1;
                            }

                            gs->handcuffs_selecting = 0;
                            gs->steal_target_idx = -1;
                        } else {
                            game_log(gs, "Handcuffs cancelled.");
                            gs->handcuffs_selecting = 0;
                            gs->steal_target_idx = -1;
                        }
                        break;
                    }

                    // --- ADRENALINE CONFIRM ---
                    if (gs->adrenaline_steal_phase) {
                        int victim = gs->steal_target_idx;
                        int slot = gs->adrenaline_steal_slot;
                        ItemType item_to_steal = gs->players[victim].items[slot];

                        if (item_to_steal != ITEM_NONE && item_to_steal != ITEM_ADRENALINE) {
                            ap.action_type = 2;
                            ap.item_idx = slot;
                            ap.target_idx = victim;
                            ap.actor_idx = gs->local_player_index;

                            if (net_ctx->is_host) {
                                int log_before = gs->n_log;
                                apply_action(gs, &ap, gwin);
                                for (int l = log_before; l < gs->n_log; l++) {
                                    if (strncmp(gs->log[l], "[PRIVATE] ", 10) == 0) {
                                        memmove(gs->log[l], gs->log[l] + 10, strlen(gs->log[l] + 10) + 1);
                                        send_log_to_player(net_ctx, ap.actor_idx, gs->log[l]);
                                    } else {
                                        broadcast_log(net_ctx, gs->log[l]);
                                    }
                                }
                                broadcast_action(net_ctx, &ap);
                                send_sync(net_ctx, gs);
                            } else {
                                send_action_to_host(net_ctx, gs, &ap);
                                gs->awaiting_sync = 1;
                            }
                        } else {
                            game_log(gs, "Can't steal that item.");
                        }

                        gs->adrenaline_steal_phase = 0;
                        gs->adrenaline_steal_slot = 0;
                        gs->steal_target_idx = -1;
                        break;
                    }

                    if (gs->adrenaline_active) {
                        if (gs->steal_target_idx != -1) {
                            gs->adrenaline_active = 0;
                            gs->adrenaline_steal_phase = 1;
                            gs->adrenaline_steal_slot = 0;
                            game_log(gs, "ADRENALINE: Pick item to steal (1-8 or Up/Down, Enter)");
                        }
                        break;
                    }

                    // --- NORMAL ACTION LOGIC ---
                    int target_player = -1;

                    if (gs->selected_action < shoot_options) {
                        if (gs->selected_action == (shoot_options - 1)) {
                            target_player = gs->local_player_index;
                        } else {
                            int opp_count = 0;
                            for (int pl = 0; pl < gs->player_count; pl++) {
                                if (pl == gs->local_player_index) continue;

                                if (opp_count == gs->selected_action) {
                                    target_player = pl;
                                    break;
                                }
                                opp_count++;
                            }
                        }
                    }

                    if (target_player != -1) {
                        ap.action_type = 0;
                        ap.target_idx = target_player;
                        ap.actor_idx = gs->local_player_index;

                        if (g_debug_mode) game_log(gs, "[DEBUG] -> SENDING SHOOT PACKET");

                        if (net_ctx->is_host) {
                            int log_before = gs->n_log;
                            apply_action(gs, &ap, gwin);
                            for (int l = log_before; l < gs->n_log; l++) {
                                if (strncmp(gs->log[l], "[PRIVATE] ", 10) == 0) {
                                    memmove(gs->log[l], gs->log[l] + 10, strlen(gs->log[l] + 10) + 1);
                                    send_log_to_player(net_ctx, ap.actor_idx, gs->log[l]);
                                } else {
                                    broadcast_log(net_ctx, gs->log[l]);
                                }
                            }
                            broadcast_action(net_ctx, &ap);
                            send_sync(net_ctx, gs);
                        } else {
                            send_action_to_host(net_ctx, gs, &ap);
                            gs->awaiting_sync = 1;
                        }
                    } else {
                        if (gs->selected_item >= 0 && gs->selected_item < MAX_ITEMS) {
                            Player *p = &gs->players[gs->local_player_index];
                            ItemType item = p->items[gs->selected_item];

                            if (item == ITEM_ADRENALINE) {
                                // Check there's something to steal first
                                int has_stealable = 0;
                                for (int i = 0; i < gs->player_count; i++) {
                                    if (i == gs->local_player_index) continue;
                                    for (int k = 0; k < MAX_ITEMS; k++) {
                                        if (gs->players[i].items[k] != ITEM_NONE &&
                                            gs->players[i].items[k] != ITEM_ADRENALINE) {
                                            has_stealable = 1;
                                            break;
                                        }
                                    }
                                    if (has_stealable) break;
                                }
                                if (!has_stealable) {
                                    game_log(gs, "Nothing to steal!");
                                } else {
                                    // Enter selection mode. Actual use_item fires in ADRENALINE CONFIRM.
                                    gs->adrenaline_active = 1;
                                    gs->steal_target_idx = -1;
                                    for (int i = 0; i < gs->player_count; i++) {
                                        if (i != gs->local_player_index) {
                                            gs->steal_target_idx = i;
                                            break;
                                        }
                                    }
                                    game_log(gs, "ADRENALINE: Select target (Up/Down, Enter)");
                                }
                            } else if (item == ITEM_HANDCUFFS) {
                                // Enter target-selection mode. The actual item use and
                                // network packet are sent in the HANDCUFFS CONFIRM block.
                                gs->handcuffs_selecting = 1;
                                gs->steal_target_idx = -1;
                                for (int i = 0; i < gs->player_count; i++) {
                                    if (i != gs->local_player_index) {
                                        gs->steal_target_idx = i;
                                        break;
                                    }
                                }
                                game_log(gs, "HANDCUFFS: Select target (Up/Down, Enter)");
                            } else {
                                if (g_debug_mode) {
                                    char dbuf[64];
                                    snprintf(dbuf, 64, "[DEBUG] Sending Item %d (%s)",
                                             gs->selected_item, item_name(item));
                                    game_log(gs, dbuf);
                                }

                                ap.action_type = 1;
                                ap.item_idx = gs->selected_item;
                                ap.target_idx = -1;
                                ap.actor_idx = gs->local_player_index;

                                if (net_ctx->is_host) {
                                    int log_before = gs->n_log;
                                    apply_action(gs, &ap, gwin);
                                    for (int l = log_before; l < gs->n_log; l++) {
                                        if (strncmp(gs->log[l], "[PRIVATE] ", 10) == 0) {
                                            memmove(gs->log[l], gs->log[l] + 10, strlen(gs->log[l] + 10) + 1);
                                            send_log_to_player(net_ctx, ap.actor_idx, gs->log[l]);
                                        } else {
                                            broadcast_log(net_ctx, gs->log[l]);
                                        }
                                    }
                                    broadcast_action(net_ctx, &ap);
                                    send_sync(net_ctx, gs);
                                } else {
                                    send_action_to_host(net_ctx, gs, &ap);
                                    gs->awaiting_sync = 1;
                                }
                            }
                        } else {
                            game_log(gs, "No item selected! Press 1-8.");
                        }
                    }
                    break;
            }
        }
    }
done:
    cbreak();
    delwin(gwin);
    clear();
    refresh();
}

void render_host_admin_console(GameState *gs, NetworkContext *net_ctx) {
    WINDOW *gwin = newwin(LINES, COLS, 0, 0);
    box(gwin, 0, 0);
    keypad(gwin, TRUE);
    halfdelay(5);

    fd_set readfds, exceptfds;
    struct timeval tv;
    int max_sd;

    while (1) {
        werase(gwin);

        // --- DRAW INTERFACE ---
        wattron(gwin, A_BOLD | COLOR_PAIR(4));
        mvwprintw(gwin, 1, 2, "HOST ADMIN CONSOLE");
        wattroff(gwin, A_BOLD | COLOR_PAIR(4));

        const char *gun_status = gs->saw_active ? "SAWN-OFF (2x DMG)" : "NORMAL";
        int status_color = gs->saw_active ? COLOR_PAIR(3) : COLOR_PAIR(2);

        wattron(gwin, A_BOLD | status_color);
        mvwprintw(gwin, 1, COLS - 25, "Gun Status: %s", gun_status);
        wattroff(gwin, A_BOLD | status_color);

        mvwprintw(gwin, 1, COLS - 18, "Round %d/%d", gs->round, gs->roundAmount);

        mvwprintw(gwin, 3, 2, "Shell Sequence (Current in RED):");

        int print_x = 2;
        for (int i = 0; i < gs->n_shells; i++) {
            char shell_char = (gs->shells[i] == SHELL_LIVE) ? 'L' : 'B';

            int is_current = (i == gs->current_shell);
            int is_dangerous = (gs->saw_active && gs->shells[i] == SHELL_LIVE);

            if (is_current) {
                wattron(gwin, A_REVERSE | COLOR_PAIR(1));
                if (is_dangerous) wattron(gwin, COLOR_PAIR(3));
                mvwprintw(gwin, 4, print_x, "%c", shell_char);
                wattroff(gwin, A_REVERSE | COLOR_PAIR(1) | COLOR_PAIR(3));
            } else {
                wattron(gwin, A_DIM);
                if (i > gs->current_shell) wattroff(gwin, A_DIM);
                mvwprintw(gwin, 4, print_x, "%c", shell_char);
                wattroff(gwin, A_DIM);
            }
            print_x += 2;
        }

        mvwhline(gwin, 6, 1, ACS_HLINE, COLS - 2);

        mvwprintw(gwin, 7, 2, "Game Log History:");

        int log_row = 9;
        int max_visible_logs = LINES - 12;
        int start_log_idx = 0;

        if (gs->n_log > max_visible_logs) {
            start_log_idx = gs->n_log - max_visible_logs;
        }

        for (int i = start_log_idx; i < gs->n_log; i++) {
            mvwprintw(gwin, log_row++, 2, "%s", gs->log[i]);
        }

        box(gwin, 0, 0);
        wrefresh(gwin);

        // --- GAME OVER CHECK ---
        int alive_count = 0;
        for(int i=0; i<gs->player_count; i++) {
            if(gs->players[i].charges > 0) alive_count++;
        }

        if (alive_count <= 1) {
            napms(2000);

            int winner_idx = -1;
            for(int i = 0; i < gs->player_count; i++) {
                if(gs->players[i].charges > 0) winner_idx = i;
            }

            if (gs->round >= gs->roundAmount) {
                char winner_msg[64];
                snprintf(winner_msg, 64, "WINNER: %s", winner_idx != -1 ? gs->players[winner_idx].name : "NONE");

                pthread_mutex_lock(&net_ctx->mutex);
                strncpy(net_ctx->log[net_ctx->log_index], winner_msg, 64);
                net_ctx->log_index = (net_ctx->log_index + 1) % MAX_NET_LOG;
                pthread_mutex_unlock(&net_ctx->mutex);

                break;
            } else {
                start_next_round(gs);
                send_sync(net_ctx, gs);
            }
        }

        // --- ACTIVE CLEANUP (Ghost Sockets) ---
        pthread_mutex_lock(&net_ctx->mutex);
        int i = 0;
        while (i < net_ctx->client_sockets_count) {
            socket_t sd = net_ctx->clients[i];
            if (sd != INVALID_SOCK) {
                fd_set probe_fds;
                FD_ZERO(&probe_fds);
                FD_SET(sd, &probe_fds);
                struct timeval probe_tv = {0, 0};
#ifdef _WIN32
                int probe = select(0, &probe_fds, NULL, NULL, &probe_tv);
#else
                int probe = select((int)sd + 1, &probe_fds, NULL, NULL, &probe_tv);
#endif
                if (probe < 0) {
                    // select() itself failed - socket is broken
                    int err = GET_SOCKET_ERROR();
                    if (g_debug_mode) {
                        char dbuf[64];
                        snprintf(dbuf, 64, "[HOST] Ghost probe select err %d on sock %d", err, (int)sd);
                        game_log(gs, dbuf);
                    } else {
                        game_log(gs, "[HOST] Ghost socket removed (select error).");
                    }
                    CLOSE_SOCKET(sd);
                    for (int j = i; j < net_ctx->client_sockets_count - 1; j++) {
                        net_ctx->clients[j] = net_ctx->clients[j+1];
                        net_ctx->player_names[j+1][0] = '\0';
                        strncpy(net_ctx->player_names[j+1], net_ctx->player_names[j+2], 32);
                    }
                    net_ctx->client_sockets_count--;
                    continue;
                } else if (probe > 0 && FD_ISSET(sd, &probe_fds)) {
                    char dummy;
                    int res = recv(sd, &dummy, 1, MSG_PEEK);
                    if (res == 0) {
                        if (g_debug_mode) {
                            char dbuf[64];
                            snprintf(dbuf, 64, "[HOST] Ghost sock %d: peer closed (EOF)", (int)sd);
                            game_log(gs, dbuf);
                        } else {
                            game_log(gs, "[HOST] Ghost socket removed (closed).");
                        }
                        CLOSE_SOCKET(sd);
                        for (int j = i; j < net_ctx->client_sockets_count - 1; j++) {
                            net_ctx->clients[j] = net_ctx->clients[j+1];
                            net_ctx->player_names[j+1][0] = '\0';
                            strncpy(net_ctx->player_names[j+1], net_ctx->player_names[j+2], 32);
                        }
                        net_ctx->client_sockets_count--;
                        continue;
                    } else if (res < 0) {
                        int err = GET_SOCKET_ERROR();
                        if (err != EAGAIN && err != EWOULDBLOCK) {
                            if (g_debug_mode) {
                                char dbuf[64];
                                snprintf(dbuf, 64, "[HOST] Ghost sock %d: recv peek err %d", (int)sd, err);
                                game_log(gs, dbuf);
                            } else {
                                game_log(gs, "[HOST] Ghost socket removed (error).");
                            }
                            CLOSE_SOCKET(sd);
                            for (int j = i; j < net_ctx->client_sockets_count - 1; j++) {
                                net_ctx->clients[j] = net_ctx->clients[j+1];
                                net_ctx->player_names[j+1][0] = '\0';
                                strncpy(net_ctx->player_names[j+1], net_ctx->player_names[j+2], 32);
                            }
                            net_ctx->client_sockets_count--;
                            continue;
                        }
                    }
                }
            }
            i++;
        }
        pthread_mutex_unlock(&net_ctx->mutex);

        // --- SELECT SETUP ---
        FD_ZERO(&readfds);
        FD_ZERO(&exceptfds);
        max_sd = 0;

        pthread_mutex_lock(&net_ctx->mutex);
        for (int i = 0; i < net_ctx->client_sockets_count; i++) {
            socket_t sd = net_ctx->clients[i];
            if (sd != INVALID_SOCK) {
                FD_SET(sd, &readfds);
                FD_SET(sd, &exceptfds);
                if ((int)sd > max_sd) max_sd = (int)sd;
            }
        }
        pthread_mutex_unlock(&net_ctx->mutex);

        tv.tv_sec = 1;
        tv.tv_usec = 0;

        // --- SELECT LOOP ---
        int activity = 0;
        if (max_sd > 0) {
            activity = select(max_sd + 1, &readfds, NULL, &exceptfds, &tv);
        }

        if (activity < 0) {
            napms(1000);
            continue;
        }

        // --- PROCESS INPUT ---
        int ch = wgetch(gwin);
        if (ch != ERR) {
            if (ch == 'q') {
                net_ctx->should_stop = 1;
                break;
            }
        }

        // --- PROCESS EXCEPTIONS (Socket Errors/OOB) ---
        if (activity > 0) {
            pthread_mutex_lock(&net_ctx->mutex);
            for (int i = 0; i < net_ctx->client_sockets_count; i++) {
                socket_t sd = net_ctx->clients[i];
                if (sd != INVALID_SOCK && FD_ISSET(sd, &exceptfds)) {
                    if (g_debug_mode) {
                        char dbuf[64];
                        snprintf(dbuf, 64, "[HOST] Exception on sock %d (err %d)", (int)sd, GET_SOCKET_ERROR());
                        game_log(gs, dbuf);
                    } else {
                        game_log(gs, "[HOST] Socket exception detected.");
                    }
                    CLOSE_SOCKET(sd);

                     for (int j = i; j < net_ctx->client_sockets_count - 1; j++) {
                        net_ctx->clients[j] = net_ctx->clients[j+1];
                        net_ctx->player_names[j+1][0] = '\0';
                        strncpy(net_ctx->player_names[j+1], net_ctx->player_names[j+2], 32);
                     }
                     net_ctx->client_sockets_count--;
                     // Don't increment 'i'
                } else {
                    i++;
                }
            }
            pthread_mutex_unlock(&net_ctx->mutex);
        }

        // --- PROCESS DATA (Incoming Packets) ---
        if (activity > 0) {
            pthread_mutex_lock(&net_ctx->mutex);
            for (int i = 0; i < net_ctx->client_sockets_count; i++) {
                socket_t sd = net_ctx->clients[i];

                if (sd == INVALID_SOCK) continue;
                if (FD_ISSET(sd, &readfds)) {

                    // DEBUG
                    if (g_debug_mode) {
                        char dbuf[64];
                        snprintf(dbuf, 64, "[HOST] DATA on Socket %d", (int)sd);
                        game_log(gs, dbuf);
                    }

                    // CRITICAL: UNLOCK MUTEX before recv
                    pthread_mutex_unlock(&net_ctx->mutex);

                    char header;
                    ActionPacket ap;
                    int res = recv_packet(sd, &header, &ap, sizeof(ap), gs);

                    if (res == 0) {
                        // --- PACKET RECEIVED SUCCESSFULLY ---
                        if (header == PKT_ACTION) {
                            apply_action(gs, &ap, gwin);

                            if (gs->current_shell >= gs->n_shells) {
                                game_log(gs, "Gun empty. Reloading...");
                                generate_shells(gs);
                                give_new_items(gs);
                                gs->show_shells = 1;
                                gs->shell_reveal_time = time(NULL);
                            }

                            if (gs->n_log > 0) {
                                broadcast_log(net_ctx, gs->log[gs->n_log - 1]);
                            }

                            broadcast_action(net_ctx, &ap);
                            send_sync(net_ctx, gs);

                            goto DONE_DATA;
                        } else {
                            game_log(gs, "[HOST] Unknown packet.");
                        }
                    } else if (res == -2) {
                        // Just continue to next select loop.
                    } else {
                        // --- PACKET RECV FAILED (Error or Disconnect) ---
                        // res == -1
                        pthread_mutex_lock(&net_ctx->mutex);
                        if (g_debug_mode) {
                            char dbuf[64];
                            snprintf(dbuf, 64, "[HOST] sock %d recv failed (err %d)", (int)sd, GET_SOCKET_ERROR());
                            game_log(gs, dbuf);
                        } else {
                            game_log(gs, "[HOST] Client disconnected or recv error.");
                        }

                        CLOSE_SOCKET(sd);

                        // Shift array
                        for (int j = i; j < net_ctx->client_sockets_count - 1; j++) {
                            net_ctx->clients[j] = net_ctx->clients[j+1];
                            net_ctx->player_names[j+1][0] = '\0';
                            strncpy(net_ctx->player_names[j+1], net_ctx->player_names[j+2], 32);
                        }
                        net_ctx->client_sockets_count--;
                        pthread_mutex_unlock(&net_ctx->mutex);

                        goto DONE_DATA;
                    }
                }
            }
            pthread_mutex_unlock(&net_ctx->mutex);
        }

        DONE_DATA:;
    }

    cbreak();
    delwin(gwin);
    clear();
    refresh();
}


// --- Spectator Mode (Unused by Host, kept for reference) ---
void render_spectator_game(GameState *gs, NetworkContext *net_ctx) {
    WINDOW *gwin = newwin(LINES, COLS, 0, 0);
    box(gwin, 0, 0);
    keypad(gwin, TRUE);
    halfdelay(1);

    int ch;
    while (1) {
        gs->player_turn = 0;

        draw_main_ui(gwin, gs);

        if (net_ctx->sock == INVALID_SOCK) {
            game_log(gs, "Disconnected.");
            napms(2000);
            break;
        }

        int alive_count = 0;
        for(int i=0; i<gs->player_count; i++) {
            if(gs->players[i].charges > 0) alive_count++;
        }

        if (alive_count <= 1) {
            napms(2000);
            break;
        }

        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(net_ctx->sock, &readfds);

        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 100000;

        int activity = select((int)net_ctx->sock + 1, &readfds, NULL, NULL, &tv);

        if (activity > 0 && FD_ISSET(net_ctx->sock, &readfds)) {
             char header;
             ActionPacket ap;
             if (recv_packet(net_ctx->sock, &header, &ap, sizeof(ap), gs) == 0 && header == PKT_ACTION) {
                 apply_action(gs, &ap, gwin);
             } else {
                 game_log(gs, "Disconnected.");
                 break;
             }
        }

        ch = wgetch(gwin);
        if (ch != ERR) {
            if (ch == 'q') break;
        }
    }

    cbreak();
    delwin(gwin);
    clear();
    refresh();
}

// --- Offline / Start Game Menu ---
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

int show_win_screen(int current_money, int cigs, int shells, int damage, int beer) {
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

        mvwprintw(win, 4, 2, "Money Earned:   $%d", current_money);
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
                    return current_money * 2;
                } else {
                    return current_money;
                }
            case 'q':
                delwin(win);
                return current_money;
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
            int current_money = 10000;
            int running = 1;
            while(running) {
                GameState gs;
                init_game(&gs);

                int playing = 1;
                while (playing && gs.round <= gs.roundAmount) {
                    int result = render_game(&gs);

                    if (result == 0) {
                        playing = 0;
                        running = 0;
                    }
                    else if (result == 1) {
                        if (gs.players[0].charges <= 0) {
                            int death_choice = show_death_screen();
                            if (death_choice == 0) {
                                current_money = 10000;
                                init_game(&gs);
                            } else {
                                playing = 0;
                                running = 0;
                            }
                        }
                        else if (gs.players[1].charges <= 0) {
                            if (gs.round >= gs.roundAmount) {
                                int win_choice = show_win_screen(current_money, gs.cigarettes_smoked, gs.shells_ejected, gs.damage_dealt, gs.beer_ml);
                                if (win_choice == 0) {
                                    current_money *= 2;
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

void setup_player_positions(GameState *gs) {
    int positions[4] = { POS_BOTTOM, POS_RIGHT, POS_TOP, POS_LEFT };
    int opponent_indices[MAX_PLAYERS];
    int opponent_count = 0;

    for (int i = 0; i < gs->player_count; i++) {
        if (i != gs->local_player_index) {
            opponent_indices[opponent_count++] = i;
        }
    }

    for (int i = opponent_count - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        int tmp = opponent_indices[i];
        opponent_indices[i] = opponent_indices[j];
        opponent_indices[j] = tmp;
    }

    gs->visual_map[POS_BOTTOM] = gs->local_player_index;

    if (gs->player_count >= 2) {
        gs->visual_map[POS_TOP] = opponent_indices[0];
    }

    if (gs->player_count >= 3) {
        gs->visual_map[POS_RIGHT] = opponent_indices[0];
        gs->visual_map[POS_LEFT]  = opponent_indices[1];
    }

    if (gs->player_count == 4) {
        gs->visual_map[POS_RIGHT] = opponent_indices[0];
        gs->visual_map[POS_TOP]   = opponent_indices[1];
        gs->visual_map[POS_LEFT]  = opponent_indices[2];
    }
}

void draw_main_ui(WINDOW *gwin, GameState *gs) {
    werase(gwin);
    box(gwin, 0, 0);

    int mid_y = LINES / 2;
    int mid_x = COLS / 2;

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
        if (gs->player_count >= 3) {
            mvwprintw(gwin, gun_y + 1, mid_x - 6, "Sawn-off: %s", gs->saw_active ? "TRUE" : "FALSE");
        } else {
            mvwprintw(gwin, gun_y + 1, mid_x - 8, "LIVE: ?  BLANK: ?");
        }
    }

    char turn_name[33];
    int turn_idx = gs->current_player_index;
    strncpy(turn_name, gs->players[turn_idx].name, 32);

    int turn_text_w = strlen(turn_name) + 7;

    if (gs->player_count >= 3) {
        wattron(gwin, A_BOLD | A_UNDERLINE | COLOR_PAIR(2));
        mvwprintw(gwin, gun_y + 2, mid_x - turn_text_w/2, "%s's TURN", turn_name);
        wattroff(gwin, A_BOLD | A_UNDERLINE | COLOR_PAIR(2));

        char round_buf[32];
        snprintf(round_buf, sizeof(round_buf), "Round %d / %d", gs->round, gs->roundAmount);

        wattron(gwin, A_BOLD);
        mvwprintw(gwin, gun_y + 3, mid_x - strlen(round_buf)/2, "%s", round_buf);
        wattroff(gwin, A_BOLD);
    } else {
        wattron(gwin, A_BOLD | A_UNDERLINE | COLOR_PAIR(2));
        mvwprintw(gwin, mid_y - 2, mid_x - turn_text_w/2, "%s's TURN", turn_name);
        wattroff(gwin, A_BOLD | A_UNDERLINE | COLOR_PAIR(2));

        mvwprintw(gwin, mid_y - 1, mid_x - 12,
                  "Round %d/%d  |  Sawn off: %s",
                  gs->round, gs->roundAmount, gs->saw_active ? "TRUE" : "FALSE");
    }

    int log_anchor_x, log_anchor_y;
    int act_anchor_x, act_anchor_y;

    int is_compact_layout = (gs->player_count >= 2);

    if (is_compact_layout) {
        log_anchor_x = mid_x - 40;
        act_anchor_x = mid_x + 10;

        log_anchor_y = gun_y + 4;
        act_anchor_y = gun_y + 4;
    } else {
        log_anchor_x = 2;
        log_anchor_y = mid_y - 7;
        act_anchor_x = COLS - 32;
        act_anchor_y = mid_y - 4;
    }

    int log_h = 14;
    WINDOW *log_win = derwin(gwin, log_h, 30, log_anchor_y, log_anchor_x);
    box(log_win, 0, 0);
    mvwprintw(log_win, 0, 2, " LOG ");
    int log_start = (gs->n_log > log_h - 2) ? gs->n_log - (log_h - 2) : 0;
    for (int i = log_start; i < gs->n_log; i++)
        mvwprintw(log_win, i - log_start + 1, 1, "%-28s", gs->log[i]);
    wrefresh(log_win);
    delwin(log_win);

    struct { int y; int x; } coords[4] = {
        { LINES - 5, mid_x },     // Bottom (Local)
        { mid_y,      COLS - 20 }, // Right
        { 2,          mid_x },     // Top
        { mid_y,      5 }          // Left
    };

    int my_global_pos = -1;
    for (int pos = 0; pos < 4; pos++) {
        if (gs->visual_map[pos] == gs->local_player_index) {
            my_global_pos = pos;
            break;
        }
    }
    int shift = 0 - my_global_pos;

    for (int p_idx = 0; p_idx < gs->player_count; p_idx++) {
        Player *p = &gs->players[p_idx];
        int is_me = (p_idx == gs->local_player_index);
        int is_turn = (p_idx == gs->current_player_index);

        int global_pos = -1;
        for (int gp = 0; gp < 4; gp++) {
            if (gs->visual_map[gp] == p_idx) {
                global_pos = gp;
                break;
            }
        }

        int screen_pos = (global_pos + shift + 4) % 4;

        int cy = coords[screen_pos].y;
        int cx = coords[screen_pos].x;

        if (screen_pos == POS_TOP) mvwhline(gwin, cy + 2, 1, ACS_HLINE, COLS - 2);
        if (screen_pos == POS_BOTTOM) mvwhline(gwin, cy - 1, 1, ACS_HLINE, COLS - 2);

        if (gs->cuffs_target == p_idx) {
             wattron(gwin, COLOR_PAIR(1) | A_BOLD);
             mvwprintw(gwin, cy - 2, cx - 10, "[HANDCUFFED]");
             wattroff(gwin, COLOR_PAIR(1) | A_BOLD);
        }

        if (is_turn) wattron(gwin, COLOR_PAIR(2) | A_BOLD);
        else wattron(gwin, A_BOLD);

        int name_y = cy - 1;
        if (screen_pos == POS_BOTTOM) {
            name_y = cy - 2;
        }

        char name_tag[10];
        if (is_me) strcpy(name_tag, "[YOU]");
        else snprintf(name_tag, 10, "[P%d]", p_idx + 1);

        mvwprintw(gwin, name_y, cx, "%s %s", name_tag, p->name);

        if (is_turn) wattroff(gwin, COLOR_PAIR(2) | A_BOLD);
        else wattroff(gwin, A_BOLD);

        draw_charges(gwin, cy + 1, cx, p->charges, p->max_charges);

        // --- ITEM DRAWING ---
        int selection_to_use = -1;
        if (gs->adrenaline_active && is_me) {
            for(int k=0; k<MAX_ITEMS; k++) {
                if(p->items[k]==ITEM_ADRENALINE) { selection_to_use = k; break; }
            }
        } else if (gs->handcuffs_selecting && is_me) {
            for(int k=0; k<MAX_ITEMS; k++) {
                if(p->items[k]==ITEM_HANDCUFFS) { selection_to_use = k; break; }
            }
        } else if (is_me) {
            selection_to_use = gs->selected_item;
        }

        if (is_me) {
            mvwprintw(gwin, cy + 1, cx - 20, "Your items (1-8):");

            int item_w = 11;
            int cols = 4;
            int rows = 2;
            int start_x = cx - 15;
            int start_y = cy + 2;

            for (int r = 0; r < rows; r++) {
                for (int c = 0; c < cols; c++) {
                    int idx = r * cols + c;
                    int x = start_x + c * item_w;

                    wattron(gwin, A_BOLD);
                    if (gs->selected_item == idx) wattron(gwin, A_REVERSE);

                    mvwprintw(gwin, start_y + r, x, "[%-8s]", item_name(p->items[idx]));

                    wattroff(gwin, A_REVERSE);
                    wattroff(gwin, A_BOLD);
                }
            }
        } else if (screen_pos == POS_TOP) {
            mvwprintw(gwin, cy + 1, cx - 20, "Items:");
            draw_items(gwin, cy + 2, cx - 15, p->items, selection_to_use, 1);
        } else {
            int item_w = 11;
            int cols = 2;
            int rows = 4;
            int block_w = (item_w * cols);

            int start_x;

            if (screen_pos == POS_RIGHT) {
                start_x = cx - block_w - 2;
            } else {
                start_x = cx;
            }

            mvwprintw(gwin, cy + 1, start_x + (block_w/2) - 3, "Items:");

            for (int row = 0; row < rows; row++) {
                for (int col = 0; col < cols; col++) {
                    int idx = row * cols + col;
                    if (idx >= MAX_ITEMS) break;

                    int x = start_x + col * item_w;

                    wattron(gwin, A_BOLD);
                    if (selection_to_use == idx) wattron(gwin, A_REVERSE);

                    mvwprintw(gwin, cy + 2 + row, x, "[%-8s]", item_name(p->items[idx]));

                    wattroff(gwin, A_REVERSE);
                    wattroff(gwin, A_BOLD);
                }
            }
        }
    }

    // --- ACTIONS BOX DRAWING ---
    int act_h = 12;
    int act_w = 30;
    WINDOW *act_win = derwin(gwin, act_h, act_w, act_anchor_y, act_anchor_x);
    box(act_win, 0, 0);

    if (gs->current_player_index == gs->local_player_index) {
        mvwprintw(act_win, 0, 2, " ACTIONS ");

        if (gs->adrenaline_steal_phase) {
            Player *victim = &gs->players[gs->steal_target_idx];
            mvwprintw(act_win, 1, 1, "Steal from %s:", victim->name);

            int row = 2;
            for (int k = 0; k < MAX_ITEMS; k++) {
                int is_selected = (k == gs->adrenaline_steal_slot);
                int is_stealable = (victim->items[k] != ITEM_NONE &&
                                    victim->items[k] != ITEM_ADRENALINE);

                if (is_selected && is_stealable) {
                    wattron(act_win, A_REVERSE | COLOR_PAIR(2));
                    mvwprintw(act_win, row++, 1, "%d: %-9s (STEAL)", k+1, item_name(victim->items[k]));
                    wattroff(act_win, A_REVERSE | COLOR_PAIR(2));
                } else if (is_selected) {
                    wattron(act_win, A_REVERSE);
                    mvwprintw(act_win, row++, 1, "%d: %-9s", k+1, item_name(victim->items[k]));
                    wattroff(act_win, A_REVERSE);
                } else if (!is_stealable) {
                    wattron(act_win, A_DIM);
                    mvwprintw(act_win, row++, 1, "%d: %-9s", k+1, item_name(victim->items[k]));
                    wattroff(act_win, A_DIM);
                } else {
                    mvwprintw(act_win, row++, 1, "%d: %-9s", k+1, item_name(victim->items[k]));
                }
            }
            mvwprintw(act_win, row + 1, 1, "1-8/Up/Down, Enter");
            mvwprintw(act_win, row + 2, 1, "ESC to Cancel");
        } else if (gs->adrenaline_active || gs->handcuffs_selecting) {
            // SELECTION MODE (Adrenaline or Handcuffs)
            if (gs->handcuffs_selecting) {
                mvwprintw(act_win, 1, 1, "Cuff Player:");
            } else {
                mvwprintw(act_win, 1, 1, "Steal from:");
            }

            int row = 2;
            int opp_count = 0;

            for (int i = 0; i < gs->player_count; i++) {
                if (i == gs->local_player_index) continue;

                char name[20];
                snprintf(name, 20, "[P%d] %s", i, gs->players[i].name);

                // For adrenaline, check if this opponent has anything stealable
                int has_stealable = 1;
                if (gs->adrenaline_active) {
                    has_stealable = 0;
                    for (int k = 0; k < MAX_ITEMS; k++) {
                        if (gs->players[i].items[k] != ITEM_NONE &&
                            gs->players[i].items[k] != ITEM_ADRENALINE) {
                            has_stealable = 1;
                            break;
                        }
                    }
                }

                if (i == gs->steal_target_idx) {
                    wattron(act_win, A_REVERSE | COLOR_PAIR(2));
                    mvwprintw(act_win, row++, 1, "%s (CONFIRM)", name);
                    wattroff(act_win, A_REVERSE | COLOR_PAIR(2));
                } else if (!has_stealable) {
                    wattron(act_win, A_DIM);
                    mvwprintw(act_win, row++, 1, "%s (empty)", name);
                    wattroff(act_win, A_DIM);
                } else {
                    mvwprintw(act_win, row++, 1, "%s", name);
                }
                opp_count++;
            }
            mvwprintw(act_win, row + 1, 1, "Up/Down to Change");
            mvwprintw(act_win, row + 2, 1, "ENTER to Select");
            mvwprintw(act_win, row + 3, 1, "ESC to Cancel");
        } else {
            int menu_y = 1;

            for (int i = 0; i < gs->player_count; i++) {
                if (i != gs->local_player_index) {
                    char display_name[12];
                    snprintf(display_name, sizeof(display_name), "%.10s", gs->players[i].name);

                    // --- Dim out the selection if the target player is dead ---
                    if (gs->players[i].charges <= 0) {
                        wattron(act_win, A_DIM);
                        mvwprintw(act_win, menu_y, 1, " Shoot %-10s (DEAD) ", display_name);
                        wattroff(act_win, A_DIM);
                    } else {
                        if (gs->selected_action == menu_y - 1) wattron(act_win, A_REVERSE);
                        mvwprintw(act_win, menu_y, 1, " Shoot %-10s     ", display_name);
                        if (gs->selected_action == menu_y - 1) wattroff(act_win, A_REVERSE);
                    }
                    menu_y++;
                }
            }

            if (gs->selected_action == menu_y - 1) wattron(act_win, A_REVERSE);
            mvwprintw(act_win, menu_y, 1, " Shoot Self       ");
            if (gs->selected_action == menu_y - 1) wattroff(act_win, A_REVERSE);
            menu_y++;

            if (gs->selected_action == menu_y - 1) wattron(act_win, A_REVERSE);
            mvwprintw(act_win, menu_y, 1, " Use item   [1-8]");
            if (gs->selected_action == menu_y - 1) wattroff(act_win, A_REVERSE);
        }
    } else {
        mvwprintw(act_win, 0, 2, " WAITING ");
        mvwprintw(act_win, 1, 1, " Waiting for other players...");
    }
    wrefresh(act_win);
    delwin(act_win);

    wrefresh(gwin);
}

// --- render_game (Offline) ---
int render_game(GameState *gs) {
    WINDOW *gwin = newwin(LINES, COLS, 0, 0);
    box(gwin, 0, 0);
    keypad(gwin, TRUE);
    halfdelay(1);

    int ch;
    while (1) {
        draw_main_ui(gwin, gs);

        if (gs->show_shells) {
            ch = wgetch(gwin);
            if (ch != ERR) {
                gs->show_shells = 0;
            }
            continue;
        }

        int alive_count = 0;
        for(int i=0; i<gs->player_count; i++) {
            if(gs->players[i].charges > 0) alive_count++;
        }

        if (alive_count <= 1 || gs->players[gs->local_player_index].charges <= 0) {
            napms(2000);
            cbreak();
            delwin(gwin);
            clear();
            refresh();
            return 1;
        }

        if (gs->player_turn) {
            ch = wgetch(gwin);
            if (ch == ERR) continue;

            // --- select which item slot to steal ---
            if (gs->adrenaline_steal_phase) {
                switch (ch) {
                    case KEY_UP:
                        gs->adrenaline_steal_slot = (gs->adrenaline_steal_slot - 1 + MAX_ITEMS) % MAX_ITEMS;
                        break;
                    case KEY_DOWN:
                        gs->adrenaline_steal_slot = (gs->adrenaline_steal_slot + 1) % MAX_ITEMS;
                        break;
                    case 10: {
                        perform_steal(gs, 0, gs->steal_target_idx, gs->adrenaline_steal_slot);
                        gs->adrenaline_steal_phase = 0;
                        gs->adrenaline_active = 0;
                        break;
                    }
                    case 27:
                        gs->adrenaline_steal_phase = 0;
                        gs->adrenaline_active = 0;
                        break;
                }
                continue;
            }

            // --- confirm target (only dealer in singleplayer) ---
            if (gs->adrenaline_active) {
                switch (ch) {
                    case 10:
                        gs->adrenaline_steal_phase = 1;
                        gs->adrenaline_steal_slot  = 0;
                        break;
                    case 27:
                        gs->adrenaline_active = 0;
                        break;
                }
                continue;
            }

            // --- NORMAL INPUT ---
            switch (ch) {
                case KEY_UP:
                    gs->selected_action = (gs->selected_action > 0) ? gs->selected_action - 1 : 2;
                    break;
                case KEY_DOWN:
                    gs->selected_action = (gs->selected_action < 2) ? gs->selected_action + 1 : 0;
                    break;
                case '1': case '2': case '3': case '4':
                case '5': case '6': case '7': case '8':
                    gs->selected_action = (ch - '1') + 2;
                    gs->selected_item = (ch - '1');
                    break;
                case 10:
                    if (gs->selected_action == 0) {
                        fire_shotgun(gs, 1, gwin);
                    } else if (gs->selected_action == 1) {
                        fire_shotgun(gs, 0, gwin);
                    } else if (gs->selected_action >= 2) {
                        int item_slot = gs->selected_action - 2;
                        ItemType selected_item_type = gs->players[0].items[item_slot];
                        if (selected_item_type == ITEM_ADRENALINE) {
                            gs->adrenaline_active   = 1;
                            gs->steal_target_idx    = 1;
                            gs->adrenaline_steal_phase = 0;
                        } else {
                            use_item(gs, item_slot, 0, -1);
                        }
                    }
                    break;
                case 'q':
                    cbreak();
                    delwin(gwin);
                    return 0;
            }
        } else {
            dealer_ai_turn(gs, gwin);
        }
    }

    cbreak();
    delwin(gwin);
    clear();
    refresh();
    return 0;
}