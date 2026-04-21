#ifndef SCREENS_H
#define SCREENS_H

#include "types.h"

// Functions
void create_settings(void);
void create_loadouts(void);
void create_credits(void);
void create_lan_menu(void);
void create_start_game_menu(void);
void draw_main_ui(WINDOW *gwin, GameState *gs);
int render_game(GameState *gs);
int show_death_screen(void);
int show_win_screen(int *current_money, int cigs, int shells, int damage, int beer);

#endif