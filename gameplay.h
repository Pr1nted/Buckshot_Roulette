#ifndef GAMEPLAY_H
#define GAMEPLAY_H

#include "types.h"

// Functions
int RangeRand(int min, int max);
void game_log(GameState *gs, const char *msg);
void init_game(GameState *gs);
void start_next_round(GameState *gs);
void generate_shells(GameState *gs);
void give_new_items(GameState *gs);
void fire_shotgun(GameState *gs, int target_is_dealer, WINDOW *gwin);
void use_item(GameState *gs, int item_idx);
void dealer_ai_turn(GameState *gs, WINDOW *gwin);

#endif