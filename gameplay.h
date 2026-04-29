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

void use_item(GameState *gs, int item_idx, int actor_idx, int target_idx);
void perform_multiplayer_shoot(GameState *gs, int shooter_idx, int target_idx, WINDOW *gwin);
void dealer_ai_turn(GameState *gs, WINDOW *gwin);

void perform_steal(GameState *gs, int actor_idx, int target_idx, int steal_slot);

#endif