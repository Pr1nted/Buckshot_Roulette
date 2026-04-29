#ifndef TYPES_H
#define TYPES_H

// Cross-platform curses
#ifdef _WIN32
    #define FD_SETSIZE 1024

    // winsock2 must come before windows.h (included inside curses.h via wincon)
    // MOUSE_MOVED is defined in wincontypes.h; undef it before PDCurses redefines it
    #include <winsock2.h>
    #ifdef MOUSE_MOVED
        #undef MOUSE_MOVED
    #endif
    #include <curses.h>   // PDCurses

    #ifndef MSG_DONTWAIT
        #define MSG_DONTWAIT 0
    #endif

    #ifndef EAGAIN
        #define EAGAIN      WSAEWOULDBLOCK
    #endif
    #ifndef EWOULDBLOCK
        #define EWOULDBLOCK WSAEWOULDBLOCK
    #endif
    static inline int _sock_errno(void) { return WSAGetLastError(); }
    #define sock_errno _sock_errno()
#else
    #include <ncurses.h>  // ncurses (macOS, Linux)
    #define sock_errno errno
#endif
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define MAX_ITEMS 8
#define MAX_LOG   32
#define MAX_SHELLS 8
#define MAX_PLAYERS 4 // Updated to handle 4 players

// --- Visual Positions ---
typedef enum {
    POS_BOTTOM = 0,
    POS_RIGHT  = 1,
    POS_TOP    = 2,
    POS_LEFT   = 3
} PlayerPos;

// --- Enums ---
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

// --- Structs ---
typedef struct {
    char name[32];
    int charges;
    int max_charges;
    ItemType items[MAX_ITEMS];
} Player;

typedef struct {
    Player players[MAX_PLAYERS]; // Array of players instead of single player/dealer
    int player_count;          // Total players in game (2, 3, or 4)

    // Networking & Rendering Logic
    int local_player_index;
    int current_player_index;
    int visual_map[MAX_PLAYERS];

    // Selection Logic
    int selected_action;
    int selected_item;
    int adrenaline_active;
    int adrenaline_steal_phase;
    int adrenaline_steal_slot;
    int steal_target_idx;
    int handcuffs_selecting;

    // Game Logic
    ShellType shells[MAX_SHELLS];
    int n_shells;
    int live_count;
    int blank_count;
    int current_shell;
    int saw_active;
    int cuffs_target;          // Index of cuffed player (-1 = nobody cuffed)

    int round;
    int roundAmount;
    int player_turn;        // 1 = player's turn, 0 = dealer's turn (Legacy logic, maybe remove if fully MP)

    // Loadout configuration
    int active_loadout;      // 0, 1, or 2

    int show_shells;
    time_t shell_reveal_time;
    char log[MAX_LOG][64];
    int n_log;

    // Networking
    int awaiting_sync;
    int is_host;

    int round_wins[MAX_PLAYERS];

    // Stats
    int money;
    int cigarettes_smoked;
    int shells_ejected;
    int damage_dealt;
    int beer_ml;
} GameState;

// --- UI Structs ---
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

// --- Globals ---
extern int active_loadout;
extern int g_flashes_enabled;
extern int g_debug_mode;
extern const char *loadout_names[];
extern WINDOW *border_win;

#endif