#ifndef UI_H
#define UI_H

#include "types.h"

// Functions
void draw_border(void);
int wrap_text_into(int slot, const char *lines[], int n_lines, int max_w,
                   const char *out[], int max_out);
int create_menu(Menu m);
void draw_items(WINDOW *win, int row, int col, ItemType items[], int selected_action, int highlight);
void draw_charges(WINDOW *win, int row, int col, int charges, int max_charges);
const char *item_name(ItemType item);
void trigger_flash(WINDOW *gwin);
void init_colors(void);
void draw_main_ui(WINDOW *gwin, GameState *gs);

#endif