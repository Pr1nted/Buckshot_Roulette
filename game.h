#ifndef GAME_H
#define GAME_H

#include <ncurses.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define MAX_ITEMS 8
#define MAX_LOG   32
#define MAX_SHELLS 8

// --- Enums & Structs ---

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

typedef struct {
    const char **title_lines;
    int n_title_lines;
    const char **options;
    int n_options;
} Menu;

typedef struct {
    const char *name;
    const char **description;
    int n_desc_lines;
} Loadout;

// --- Global Variables (extern) ---

extern int active_loadout;
extern int g_flashes_enabled;
extern const char *loadout_names[];
extern WINDOW *border_win;

// --- Function Prototypes ---

// main.c
void init_colors(void);

// ui.c
void draw_border(void);
int wrap_text_into(int slot, const char *lines[], int n_lines, int max_w,
                   const char *out[], int max_out);
int create_menu(Menu m);
void draw_items(WINDOW *win, int row, int col, ItemType items[], int selected_action, int highlight);
void draw_charges(WINDOW *win, int row, int col, int charges, int max_charges);
const char *item_name(ItemType item);
void trigger_flash(WINDOW *gwin);

// gameplay.c
int RangeRand(int min, int max);
void game_log(GameState *gs, const char *msg);
void init_game(GameState *gs);
void start_next_round(GameState *gs);
void generate_shells(GameState *gs);
void give_new_items(GameState *gs);
void fire_shotgun(GameState *gs, int target_is_dealer, WINDOW *gwin);
void use_item(GameState *gs, int item_idx);
void dealer_ai_turn(GameState *gs, WINDOW *gwin);

// screens.c
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