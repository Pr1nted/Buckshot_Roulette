#ifndef TYPES_H
#define TYPES_H

#include <ncurses.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define MAX_ITEMS 8
#define MAX_LOG   32
#define MAX_SHELLS 8

// Enums

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

// Structs

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

// Global Variables

extern int active_loadout;
extern int g_flashes_enabled;
extern const char *loadout_names[];
extern WINDOW *border_win;

#endif