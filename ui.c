#include "game.h"

// Global buffer for text wrapping
static char wrap_buf[3][512][256];

void draw_border(void) {
    werase(border_win);
    box(border_win, 0, 0);
    wrefresh(border_win);
}

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