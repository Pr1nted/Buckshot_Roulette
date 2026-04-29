#ifndef SCREENS_H
#define SCREENS_H

#include "network.h"
#include "types.h"
#include "ui.h"
#include "gameplay.h"

// --- Helper ---
void show_error_popup(const char *message);

// --- Menu Prototypes ---
void create_settings(void);
void create_loadouts(void);
void create_credits(void);
void create_start_game_menu(void);
int render_game(GameState *gs);

// FIX: Changed int * to int
int show_win_screen(int current_money, int cigs, int shells, int damage, int beer);

int show_death_screen(void);

// --- LAN Prototypes ---
void create_lan_menu(void);
void create_lan_host_menu(void);
void create_lan_join_menu(void);
void run_lobby_screen(NetworkContext *net_ctx);
void run_client_lobby_screen(NetworkContext *net_ctx, const char *client_name);

// --- Game Logic Helpers ---
void setup_player_positions(GameState *gs);
void init_lan_game(GameState *gs, NetworkContext *net_ctx);
void render_multiplayer_game(GameState *gs, NetworkContext *net_ctx);
void render_spectator_game(GameState *gs, NetworkContext *net_ctx);

void render_host_admin_console(GameState *gs, NetworkContext *net_ctx);
void render_spectator_game(GameState *gs, NetworkContext *net_ctx);

#endif