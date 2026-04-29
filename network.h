#ifndef NETWORK_H
#define NETWORK_H

#include <stdbool.h>
#include <stddef.h>
#include <pthread.h>

// --- Cross-Platform Sockets ---
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>
    #include <iphlpapi.h>
    #include <stdio.h>
    typedef SOCKET socket_t;
    typedef HANDLE thread_t;
    #define INVALID_SOCK INVALID_SOCKET
    #define SOCKET_ERROR_CODE SOCKET_ERROR
    #define CLOSE_SOCKET closesocket
    #define GET_SOCKET_ERROR() WSAGetLastError()
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <ifaddrs.h>
    #include <errno.h>
    #include <sys/select.h>
    #include <signal.h>
    typedef int socket_t;
    typedef pthread_t thread_t;
    #define INVALID_SOCK -1
    #define SOCKET_ERROR_CODE -1
    #define CLOSE_SOCKET close
    #define GET_SOCKET_ERROR() errno
#endif

#define DEFAULT_PORT 12345
#define BUFFER_SIZE 1024
#define MAX_PLAYERS 4
#define MAX_NET_LOG 10

// --- Packet Definitions ---
#define PKT_JOIN 'J'
#define PKT_UPDATE 'U'
#define PKT_START 'S'
#define PKT_SYNC 'V'
#define PKT_ACTION 'A'
#define PKT_LOG   'L'   // NEW: Send log message to clients

// Payloads
typedef struct {
    int count;
    int is_host_playing;
    int rounds;
    int loadout;
    char names[MAX_PLAYERS][32];
} LobbyState;

#include "types.h"
// --- Sync Packet ---
#pragma pack(push, 1)
typedef struct {
    int n_shells;
    int live_count;
    int blank_count;
    int current_shell;
    ShellType shells[MAX_SHELLS];
    int round;
    int current_player_index;
    Player players[MAX_PLAYERS];
    int visual_map[MAX_PLAYERS];
    int show_shells;
    int cuffs_target;
} SyncPacket;

// --- Action Packet ---
typedef struct {
    int actor_idx;
    int action_type;
    int target_idx;
    int item_idx;
} ActionPacket;
#pragma pack(pop)

// --- Log Packet (New) ---
#pragma pack(push, 1)
typedef struct {
    char msg[64];
} LogPacket;
#pragma pack(pop)

// --- Order Packet ---
#pragma pack(push, 1)
typedef struct {
    ShellType shells[MAX_SHELLS];
    int n_shells;
    int round;
} OrderPacket;
#pragma pack(pop)

// --- Network Context ---
typedef struct {
    socket_t sock;
    socket_t clients[MAX_PLAYERS];
    int client_sockets_count;

    bool is_host;
    bool is_connected;
    bool should_stop;
    char remote_ip[46];

    // --- Lobby Data ---
    int player_count;
    pthread_mutex_t mutex;
    char player_names[MAX_PLAYERS][32];
    char log[MAX_NET_LOG][64];
    int log_index;
    int rounds;
    int loadout;
    char host_name[32];
    bool is_host_playing;

    // --- Game Data ---
    bool game_started;

    int self_pipe[2];
} NetworkContext;

// --- Core Functions ---
int network_init(void);
void network_cleanup(void);
char* get_local_ip(char* buffer, size_t length);

int network_start_server(NetworkContext *ctx);
int network_start_client(NetworkContext *ctx, const char *ip, const char *name);
void network_disconnect(NetworkContext *ctx);
void network_broadcast_start(NetworkContext *ctx);
void broadcast_log(NetworkContext *ctx, const char *msg);
void send_log_to_player(NetworkContext *ctx, int player_idx, const char *msg);

// --- Game Networking Functions ---
void send_sync(NetworkContext *ctx, GameState *gs);
void broadcast_action(NetworkContext *ctx, ActionPacket *ap);
void send_action_to_host(NetworkContext *ctx, GameState *gs, ActionPacket *ap);
int recv_packet(socket_t sock, char *header, void *body, size_t body_size, GameState *gs);
int recv_body(socket_t sock, void *body, size_t body_size);

#define PKT_ORDER 'O'

// --- Function Prototypes ---
void send_order(NetworkContext *ctx, GameState *gs);
int recv_order(socket_t sock, char *header, OrderPacket *op);

// --- New Prototypes for Handshake ---
int send_all(socket_t sock, const void *buf, size_t len);
int recv_blocking(socket_t sock, void *buf, size_t len);

#endif