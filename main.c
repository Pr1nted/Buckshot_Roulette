#include <ncurses.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define MAX_ITEMS 8
#define MAX_LOG   32
#define MAX_SHELLS 8

int active_loadout = 0;
int g_flashes_enabled = 0;

const char *loadout_names[] = {
    "Classic",
    "Double or nothing",
    "Pr1nted loadout+"
};

typedef struct {
    const char **title_lines;
    int n_title_lines;
    const char **options;
    int n_options;
} Menu;

WINDOW *border_win;

void draw_border(void) {
    werase(border_win);
    box(border_win, 0, 0);
    wrefresh(border_win);
}

typedef struct {
    const char *name;
    const char **description;
    int n_desc_lines;
} Loadout;

static char wrap_buf[3][512][256];

int wrap_text_into(int slot, const char *lines[], int n_lines, int max_w,
                   const char *out[], int max_out) {
    int count = 0;

    for (int i = 0; i < n_lines && count < max_out; i++) {
        const char *line = lines[i];

        if (strlen(line) == 0) {
            wrap_buf[slot][count][0] = '\0';
            out[count] = wrap_buf[slot][count];
            count++;
            continue;
        }

        int len = strlen(line);
        int start = 0;

        while (start < len && count < max_out) {
            int space = max_w - 4;
            if (start + space >= len) {
                strncpy(wrap_buf[slot][count], line + start, 255);
                wrap_buf[slot][count][255] = '\0';
                out[count] = wrap_buf[slot][count];
                count++;
                break;
            } else {
                int cut = space;
                while (cut > 0 && line[start + cut] != ' ') cut--;
                if (cut == 0) cut = space;

                strncpy(wrap_buf[slot][count], line + start, cut);
                wrap_buf[slot][count][cut] = '\0';
                out[count] = wrap_buf[slot][count];
                count++;
                start += cut + 1;
            }
        }
    }
    return count;
}

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

int create_menu(Menu m) {
    int title_w = 0;
    for (int i = 0; i < m.n_title_lines; i++) {
        int len = strlen(m.title_lines[i]);
        if (len > title_w) title_w = len;
    }
    title_w += 4;
    int title_h = m.n_title_lines + 2;

    int menu_w = 0;
    for (int i = 0; i < m.n_options; i++) {
        int len = strlen(m.options[i]);
        if (len > menu_w) menu_w = len;
    }
    menu_w += 4;
    int menu_h = m.n_options + 2;

    int title_x = (COLS - title_w) / 2;
    int title_y = (LINES / 2) - (title_h + menu_h) / 2;
    int menu_x  = (COLS - menu_w) / 2;
    int menu_y  = title_y + title_h + 1;

    WINDOW *title_win = newwin(title_h, title_w, title_y, title_x);
    WINDOW *menu_win  = newwin(menu_h,  menu_w,  menu_y,  menu_x);

    int selected = 0;
    int ch;

    while (1) {
        draw_border();

        werase(title_win);
        box(title_win, 0, 0);
        for (int i = 0; i < m.n_title_lines; i++) {
            int x = (title_w - strlen(m.title_lines[i])) / 2;

            if (i == 0) {
                wattron(title_win, A_BOLD | A_UNDERLINE);
                mvwprintw(title_win, i + 1, x, "%s", m.title_lines[i]);
                wattroff(title_win, A_BOLD | A_UNDERLINE);
            } else {
                mvwprintw(title_win, i + 1, x, "%s", m.title_lines[i]);
            }
        }
        wrefresh(title_win);

        werase(menu_win);
        box(menu_win, 0, 0);
        for (int i = 0; i < m.n_options; i++) {
            int x = (menu_w - strlen(m.options[i])) / 2;
            if (i == selected) {
                wattron(menu_win, A_REVERSE);
                mvwprintw(menu_win, i + 1, x, "%s", m.options[i]);
                wattroff(menu_win, A_REVERSE);
            } else {
                mvwprintw(menu_win, i + 1, x, "%s", m.options[i]);
            }
        }
        wrefresh(menu_win);

        ch = getch();
        switch (ch) {
            case KEY_UP:
                selected = (selected - 1 + m.n_options) % m.n_options;
                break;
            case KEY_DOWN:
                selected = (selected + 1) % m.n_options;
                break;
            case 10:
                delwin(title_win);
                delwin(menu_win);
                clear();
                refresh();
                return selected;
        }
    }
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

int RangeRand(int min, int max) {
    return rand() % (max - min + 1) + min;
}

typedef enum {
    ITEM_NONE,
    ITEM_BEER,
    ITEM_CIGARETTE,
    ITEM_HANDSAW,
    ITEM_HANDCUFFS,
    ITEM_MAGNIFYING_GLASS,
    ITEM_EXPIRED_MEDICINE,
    ITEM_INVERTER,
    ITEM_ADRENALINE,
    ITEM_BURNER_PHONE
} ItemType;

typedef enum {
    SHELL_UNKNOWN,
    SHELL_LIVE,
    SHELL_BLANK
} ShellType;

typedef struct {
    char name[32];
    int charges;
    int max_charges;
    ItemType items[MAX_ITEMS];
} Player;

typedef struct {
    Player player;
    Player dealer;
    int selected_action;
    int selected_item;
    int adrenaline_active;
    int adrenaline_slot;
    ShellType shells[MAX_SHELLS];
    int n_shells;
    int live_count;
    int blank_count;
    int current_shell;
    int saw_active;
    int cuffs_active;       // 1 = dealer is cuffed next turn
    int round;
    int roundAmount;
    int player_turn;        // 1 = player's turn, 0 = dealer's turn
    int show_shells;        // 1 = reveal shell counts briefly
    time_t shell_reveal_time;
    char log[MAX_LOG][64];
    int n_log;
    int target;             // 0 = dealer

    // Stats for Win Screen
    int money;
    int cigarettes_smoked;
    int shells_ejected;
    int damage_dealt;
    int beer_ml;
} GameState;

const char *item_name(ItemType item) {
    switch (item) {
        case ITEM_BEER:             return "Beer";
        case ITEM_CIGARETTE:        return "Cigarette";
        case ITEM_HANDSAW:          return "Handsaw";
        case ITEM_HANDCUFFS:        return "Handcuffs";
        case ITEM_MAGNIFYING_GLASS: return "Mag.Glass";
        case ITEM_EXPIRED_MEDICINE: return "Exp.Med";
        case ITEM_INVERTER:         return "Inverter";
        case ITEM_ADRENALINE:       return "Adrenaline";
        case ITEM_BURNER_PHONE:     return "Burner Ph.";
        default:                    return "---";
    }
}

void game_log(GameState *gs, const char *msg) {
    if (gs->n_log < MAX_LOG) {
        strncpy(gs->log[gs->n_log++], msg, 63);
    } else {
        // Scroll log up
        for (int i = 0; i < MAX_LOG - 1; i++)
            strncpy(gs->log[i], gs->log[i + 1], 63);
        strncpy(gs->log[MAX_LOG - 1], msg, 63);
    }
}

void draw_charges(WINDOW *win, int row, int col, int charges, int max_charges) {
    mvwprintw(win, row, col, "(%d/%d)", charges, max_charges);
}

void draw_items(WINDOW *win, int row, int col, ItemType items[], int selected_action, int highlight) {
    for (int i = 0; i < MAX_ITEMS; i++) {
        int x = col + i * 12;

        if (highlight && selected_action == i + 2)
            wattron(win, A_REVERSE);

        mvwprintw(win, row, x, "[%-9s]", item_name(items[i]));

        if (highlight && selected_action == i + 2)
            wattroff(win, A_REVERSE);
    }
}

void generate_shells(GameState *gs) {
    int total = RangeRand(2, MAX_SHELLS);

    int live  = RangeRand(1, total - 1);
    int blank = total - live;

    if (live > blank) {
        int tmp = live;
        live    = blank;
        blank   = tmp;
    }

    gs->n_shells    = total;
    gs->live_count  = live;
    gs->blank_count = blank;

    for (int i = 0; i < live; i++)
        gs->shells[i] = SHELL_LIVE;
    for (int i = live; i < total; i++)
        gs->shells[i] = SHELL_BLANK;

    for (int i = total - 1; i > 0; i--) {
        int j = RangeRand(0, i);
        ShellType tmp   = gs->shells[i];
        gs->shells[i]   = gs->shells[j];
        gs->shells[j]   = tmp;
    }

    gs->current_shell = 0;
}

void give_new_items(GameState *gs) {
    int n_to_add = RangeRand(2, 4);

    ItemType classic_pool[] = {
        ITEM_BEER, ITEM_CIGARETTE, ITEM_HANDSAW,
        ITEM_HANDCUFFS, ITEM_MAGNIFYING_GLASS
    };
    ItemType double_pool[] = {
        ITEM_BEER, ITEM_CIGARETTE, ITEM_HANDSAW, ITEM_HANDCUFFS,
        ITEM_MAGNIFYING_GLASS, ITEM_ADRENALINE, ITEM_EXPIRED_MEDICINE,
        ITEM_INVERTER, ITEM_BURNER_PHONE
    };

    ItemType *pool = (active_loadout == 1) ? double_pool : classic_pool;
    int pool_size = (active_loadout == 1) ? 9 : 5;

    Player *players[2] = {&gs->player, &gs->dealer};

    for (int p = 0; p < 2; p++) {
        int added_this_round = 0;
        for (int i = 0; i < n_to_add; i++) {
            for (int slot = 0; slot < MAX_ITEMS; slot++) {
                if (players[p]->items[slot] == ITEM_NONE) {
                    players[p]->items[slot] = pool[RangeRand(0, pool_size - 1)];
                    added_this_round++;
                    break;
                }
            }
        }
    }
    game_log(gs, "New items dealt to both sides.");
}

// FLASH LOGIC
void trigger_flash(WINDOW *gwin) {
    if (!g_flashes_enabled) return;

    chtype old_bkgd = getbkgd(gwin);

    // Set White Background
    wbkgd(gwin, COLOR_PAIR(7));
    wrefresh(gwin);

    // Wait for 0.5 seconds
    napms(500);

    // Restore background
    wbkgd(gwin, old_bkgd);
}

void fire_shotgun(GameState *gs, int target_is_dealer, WINDOW *gwin) {
    ShellType current = gs->shells[gs->current_shell];
    int damage = gs->saw_active ? 2 : 1;
    int shooter_was_player = gs->player_turn;

    if (shooter_was_player) {
        if (target_is_dealer) game_log(gs, "You aim at the DEALER...");
        else game_log(gs, "You aim at YOURSELF...");
    } else {
        if (target_is_dealer) game_log(gs, "Dealer aims at HIMSELF...");
        else game_log(gs, "Dealer aims at YOU...");
    }

    int should_swap_turn = 1;

    if (current == SHELL_LIVE) {
        game_log(gs, "BANG! It was LIVE.");
        if (target_is_dealer) {
            gs->dealer.charges -= damage;
            gs->damage_dealt += damage;
            gs->money += (2000 * damage); // Bonus money for damage
        } else {
            gs->player.charges -= damage;
            // FLASH EFFECT
            trigger_flash(gwin);
        }

        gs->live_count--;
        should_swap_turn = 1; // Live rounds always result in a turn swap
    }
    else {
        game_log(gs, "click... It was a BLANK.");
        gs->blank_count--;

        int shot_self = (shooter_was_player && !target_is_dealer) || (!shooter_was_player && target_is_dealer);

        if (shot_self) {
            game_log(gs, shooter_was_player ? "Extra turn granted!" : "Dealer keeps his turn!");
            should_swap_turn = 0;
        } else {
            should_swap_turn = 1;
        }
    }

    if (should_swap_turn) {
        if (gs->cuffs_active) {
            game_log(gs, "Handcuffs active! Turn skipped.");
            gs->cuffs_active = 0;
        } else {
            gs->player_turn = !gs->player_turn;
        }
    }

    gs->current_shell++;
    gs->saw_active = 0;

    if (gs->player.charges <= 0 || gs->dealer.charges <= 0) {
        game_log(gs, "MATCH OVER.");
    } else if (gs->current_shell >= gs->n_shells) {
        game_log(gs, "Gun empty. Reloading...");
        generate_shells(gs);
        give_new_items(gs);

        gs->show_shells = 1;
        gs->shell_reveal_time = time(NULL);
    }
}

void use_item(GameState *gs, int item_idx) {
    Player *current_p = gs->player_turn ? &gs->player : &gs->dealer;
    Player *other_p   = gs->player_turn ? &gs->dealer : &gs->player;
    ItemType item = current_p->items[item_idx];

    if (item == ITEM_NONE) return;

    char msg[64];
    snprintf(msg, sizeof(msg), "%s uses %s.", gs->player_turn ? "You" : "Dealer", item_name(item));
    game_log(gs, msg);

    switch (item) {
        case ITEM_BEER:
            if (gs->current_shell < gs->n_shells) {
                ShellType ejected = gs->shells[gs->current_shell];
                game_log(gs, ejected == SHELL_LIVE ? "Ejected a LIVE round." : "Ejected a BLANK.");
                if (ejected == SHELL_LIVE) gs->live_count--;
                else gs->blank_count--;
                gs->current_shell++;
                gs->shells_ejected++; // Track stats
                gs->beer_ml += 330; // Track Beer
            }
            break;

        case ITEM_CIGARETTE:
            if (current_p->charges < current_p->max_charges) {
                current_p->charges++;
                game_log(gs, "Gained 1 charge.");
                gs->cigarettes_smoked++; // Track cigarettes
            } else {
                game_log(gs, "Already at max charges!");
            }
            break;

        case ITEM_HANDSAW:
            if (!gs->saw_active) {
                gs->saw_active = 1;
                game_log(gs, "Shotgun sawn off! Double damage.");
            }
            break;

        case ITEM_HANDCUFFS:
            if (!gs->cuffs_active) {
                gs->cuffs_active = 1;
                game_log(gs, "Opponent cuffed! They skip a turn.");
            }
            break;

        case ITEM_MAGNIFYING_GLASS:
            if (gs->current_shell < gs->n_shells) {
                if (gs->player_turn) {
                    ShellType current = gs->shells[gs->current_shell];
                    snprintf(msg, sizeof(msg), "It's a %s.", current == SHELL_LIVE ? "LIVE round" : "BLANK");
                    game_log(gs, msg);
                } else {
                    game_log(gs, "Dealer inspects the chamber.");
                }
            }
            break;

        case ITEM_EXPIRED_MEDICINE:
            if (RangeRand(0, 1)) {
                current_p->charges = (current_p->charges + 2 > current_p->max_charges) ?
                                      current_p->max_charges : current_p->charges + 2;
                game_log(gs, "Medicine worked! +2 charges.");
            } else {
                current_p->charges--;
                game_log(gs, "Bad medicine! -1 charge.");
            }
            break;

        case ITEM_INVERTER:
            if (gs->current_shell < gs->n_shells) {
                if (gs->shells[gs->current_shell] == SHELL_LIVE) {
                    gs->shells[gs->current_shell] = SHELL_BLANK;
                    gs->live_count--; gs->blank_count++;
                } else {
                    gs->shells[gs->current_shell] = SHELL_LIVE;
                    gs->blank_count--; gs->live_count++;
                }
                game_log(gs, "Current shell inverted.");
            }
            break;

        case ITEM_BURNER_PHONE:
            if (gs->current_shell < gs->n_shells - 1) {
                if (gs->player_turn) {
                    int future_idx = RangeRand(gs->current_shell + 1, gs->n_shells - 1);
                    snprintf(msg, sizeof(msg), "Shell #%d is %s.", future_idx + 1,
                             gs->shells[future_idx] == SHELL_LIVE ? "LIVE" : "BLANK");
                    game_log(gs, msg);
                } else {
                    game_log(gs, "Dealer listens to the phone.");
                }
            } else {
                game_log(gs, gs->player_turn ? "No future shells to check." : "Dealer hears static.");
            }
            break;

        case ITEM_ADRENALINE: {
            if (gs->player_turn) {
                // Check if dealer has anything stealable (excluding adrenaline)
                int has_stealable = 0;
                for (int i = 0; i < MAX_ITEMS; i++) {
                    if (other_p->items[i] != ITEM_NONE &&
                        other_p->items[i] != ITEM_ADRENALINE) {
                        has_stealable = 1;
                        break;
                    }
                }

                if (!has_stealable) {
                    game_log(gs, "Nothing stealable! Adrenaline wasted.");
                } else {
                    gs->adrenaline_active = 1;
                    gs->selected_item = 0;
                    game_log(gs, "ADRENALINE: Select a Dealer item (1-8)");
                }
            } else {
                int stolen_idx = -1;
                for (int i = 0; i < MAX_ITEMS; i++) {
                    if (other_p->items[i] != ITEM_NONE &&
                        other_p->items[i] != ITEM_ADRENALINE) {
                        stolen_idx = i;
                        break;
                    }
                }
                if (stolen_idx != -1) {
                    ItemType stolen = other_p->items[stolen_idx];
                    for (int i = stolen_idx; i < MAX_ITEMS - 1; i++)
                        other_p->items[i] = other_p->items[i + 1];
                    other_p->items[MAX_ITEMS - 1] = ITEM_NONE;

                    for (int j = 0; j < MAX_ITEMS; j++) {
                        if (current_p->items[j] == ITEM_NONE) {
                            current_p->items[j] = stolen;
                            break;
                        }
                    }

                    // Tell the player EXACTLY what was stolen
                    snprintf(msg, sizeof(msg), "Dealer stole your %s!", item_name(stolen));
                    game_log(gs, msg);
                } else {
                    game_log(gs, "Nothing stealable! Adrenaline wasted.");
                }
            }
            break;
        }

        default: break;
    }

    for (int i = item_idx; i < MAX_ITEMS - 1; i++)
        current_p->items[i] = current_p->items[i + 1];
    current_p->items[MAX_ITEMS - 1] = ITEM_NONE;

    if (gs->current_shell >= gs->n_shells) {
        game_log(gs, "Gun empty. Reloading...");
        generate_shells(gs);
        give_new_items(gs);
        gs->show_shells = 1;
        gs->shell_reveal_time = time(NULL);
    }
}

// SHARED UI RENDERING

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

void refresh_dealer_view(GameState *gs, WINDOW *gwin) {
    draw_main_ui(gwin, gs);
    napms(1000);
}

void dealer_ai_turn(GameState *gs, WINDOW *gwin) {
    int decided_to_shoot = 0;
    int known_shell = SHELL_UNKNOWN;

    game_log(gs, "Dealer is thinking...");
    refresh_dealer_view(gs, gwin);

    while (!decided_to_shoot && gs->dealer.charges > 0 && gs->player.charges > 0) {
        int item_to_use = -1;

        // Healing
        for (int i = 0; i < MAX_ITEMS; i++) {
            if (gs->dealer.items[i] == ITEM_CIGARETTE && gs->dealer.charges < gs->dealer.max_charges) {
                item_to_use = i;
                break;
            }
        }

        // Knowledge
        if (item_to_use == -1 && known_shell == SHELL_UNKNOWN) {
            for (int i = 0; i < MAX_ITEMS; i++) {
                if (gs->dealer.items[i] == ITEM_MAGNIFYING_GLASS) {
                    item_to_use = i;
                    known_shell = gs->shells[gs->current_shell];
                    break;
                }
            }
        }

        // Offensive
        if (item_to_use == -1) {
            if (known_shell == SHELL_LIVE) {
                for (int i = 0; i < MAX_ITEMS; i++) {
                    if (gs->dealer.items[i] == ITEM_HANDSAW && !gs->saw_active) {
                        item_to_use = i;
                        break;
                    }
                }
            }
        }

        if (item_to_use != -1) {
            use_item(gs, item_to_use);
            refresh_dealer_view(gs, gwin);
        } else {
            decided_to_shoot = 1;
        }
    }

    if (gs->dealer.charges > 0 && gs->player.charges > 0) {
        if (known_shell == SHELL_LIVE) fire_shotgun(gs, 0, gwin); // Shoot player
        else if (known_shell == SHELL_BLANK) fire_shotgun(gs, 1, gwin); // Shoot self
        else {
            if (gs->live_count >= gs->blank_count) fire_shotgun(gs, 0, gwin); // Assume Live
            else fire_shotgun(gs, 1, gwin); // Assume Blank
        }
        refresh_dealer_view(gs, gwin);
    }
}

void handle_input(GameState *gs, int ch) {
    // Kept for compatibility, though logic moved to render_game switch
    if (gs->adrenaline_active) {
        switch (ch) {
            case KEY_LEFT:
                gs->selected_item = (gs->selected_item - 1 + 8) % 8;
                break;
            case KEY_RIGHT:
                gs->selected_item = (gs->selected_item + 1) % 8;
                break;
            case 10:
                gs->adrenaline_active = 0;
                break;
        }
    } else {
        switch (ch) {
            case KEY_UP:
            case KEY_DOWN:
                gs->selected_action = (gs->selected_action + 1) % 2;
                break;
            case KEY_LEFT:
                gs->selected_item = (gs->selected_item - 1 + 8) % 8;
                break;
            case KEY_RIGHT:
                gs->selected_item = (gs->selected_item + 1) % 8;
                break;
            case 10:
                break;
        }
    }
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

void deal_items(Player *p, int n_items, ItemType *pool, int pool_size) {
    // Clear all slots first
    for (int i = 0; i < MAX_ITEMS; i++)
        p->items[i] = ITEM_NONE;

    for (int i = 0; i < n_items; i++)
        p->items[i] = pool[RangeRand(0, pool_size - 1)];
}

void init_game(GameState *gs) {
    // Zero out the entire memory block.
    memset(gs, 0, sizeof(GameState));

    gs->roundAmount = 3;
    int max_charges = RangeRand(2, 4);
    int n_initial_items = RangeRand(2, 4);

    // Initialize Player
    strncpy(gs->player.name, "You", 31);
    gs->player.max_charges = max_charges;
    gs->player.charges = max_charges;

    // Initialize Dealer
    strncpy(gs->dealer.name, "Dealer", 31);
    gs->dealer.max_charges = max_charges;
    gs->dealer.charges = max_charges;

    generate_shells(gs);

    // Deal items
    give_new_items(gs);

    // Set starting state
    gs->round = 1;
    gs->player_turn = 1;
    gs->show_shells = 1;
    gs->shell_reveal_time = time(NULL);

    // Log the start
    char msg[64];
    snprintf(msg, sizeof(msg), "Round 1: %d Charges, %d Shells.", max_charges, gs->n_shells);
    game_log(gs, "Welcome to the table.");
    game_log(gs, msg);
}

void start_next_round(GameState *gs) {
    int max_charges = RangeRand(2, 4);
    gs->player.max_charges = max_charges;
    gs->dealer.max_charges = max_charges;
    gs->player.charges     = max_charges;
    gs->dealer.charges     = max_charges;

    for (int i = 0; i < MAX_ITEMS; i++) {
        gs->player.items[i] = ITEM_NONE;
        gs->dealer.items[i] = ITEM_NONE;
    }

    // Reset game state flags
    gs->saw_active        = 0;
    gs->cuffs_active      = 0;
    gs->adrenaline_active = 0;
    gs->player_turn       = 1;
    gs->selected_action   = 0;

    generate_shells(gs);
    give_new_items(gs);

    gs->show_shells       = 1;
    gs->shell_reveal_time = time(NULL);

    gs->round++;

    char msg[64];
    snprintf(msg, sizeof(msg), "Round %d: %d charges, %d shells.",
             gs->round, max_charges, gs->n_shells);
    game_log(gs, msg);
}

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
        mvwprintw(win, 8, 2, "Beer Drank:    %d ml", beer); // BEER STAT YAAAY!

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
                            if (gs.round >= gs.roundAmount) {
                                int win_choice = show_win_screen(&current_money, gs.cigarettes_smoked, gs.shells_ejected, gs.damage_dealt, gs.beer_ml);

                                if (win_choice == 0) {
                                    break;
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

int main(void) {
    srand(time(NULL));
    initscr();
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