#include "network.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

// Thread functions return DWORD on Windows, void* on POSIX
#ifdef _WIN32
    #define THREAD_RETURN return 0
#else
    #define THREAD_RETURN return NULL
#endif
#include <errno.h>
#include "gameplay.h"

#ifndef _WIN32
    #include <ifaddrs.h>
    #include <arpa/inet.h>
    #include <netdb.h>
    #include <netinet/tcp.h>  // TCP_NODELAY
    #include <fcntl.h>
#else
    #include <ws2tcpip.h>     // inet_pton, TCP_NODELAY available via winsock2.h
#endif

// --- Global Signal Handling ---
static volatile sig_atomic_t g_shutdown_requested = 0;
static NetworkContext *g_server_ctx = NULL;

void signal_handler(int sig) {
    (void)sig;
    g_shutdown_requested = 1;
}

// --- Platform Specific Initialization ---
int network_init(void) {
#ifdef _WIN32
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) return -1;
#else
    signal(SIGPIPE, SIG_IGN);
#endif
    return 0;
}

void network_cleanup(void) {
#ifdef _WIN32
    WSACleanup();
#endif
}

// --- Helper to make sockets Non-Blocking ---
void set_socket_nonblocking(socket_t sock) {
#ifdef _WIN32
    unsigned long mode = 1;
    ioctlsocket(sock, FIONBIO, &mode);
#else
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags < 0) return;
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
#endif
}

// ==========================================
// Helper: send_all
// Loops to ensure all bytes are sent on non-blocking sockets
// ==========================================
int send_all(socket_t sock, const void *buf, size_t len) {
    int total = 0;
    while (total < (int)len) {
        int sent = send(sock, (char *)buf + total, (int)len - total, 0);
        if (sent < 0) {
            int err = GET_SOCKET_ERROR();
            if (err == EAGAIN || err == EWOULDBLOCK) {
                // Buffer full, wait briefly and retry
                #ifdef _WIN32
                    Sleep(1);
                #else
                    usleep(1000);
                #endif
                continue;
            }
            return -1;
        }
        total += sent;
    }
    return 0;
}

// ==========================================
// Helper: recv_blocking (POSIX Standard)
// Loops until 'len' bytes are received.
// Used for Handshake (Login) phase.
// ==========================================
int recv_blocking(socket_t sock, void *buf, size_t len) {
    int total = 0;
    while (total < (int)len) {
        int r = recv(sock, (char *)buf + total, (int)len - total, 0);
        if (r > 0) {
            total += r;
        } else if (r == 0) {
            return -1; // Connection closed
        } else {
            // On Windows, recv might return -1 (Error) even in blocking mode
            // if the operation was interrupted. Retry.
            int err = GET_SOCKET_ERROR();
            #ifdef _WIN32
                if (err == WSAEINTR) continue;
            #else
                if (err == EINTR) continue;
            #endif
            return -1;
        }
    }
    return 0;
}

// ==========================================
// Helper: recv_body (Non-Blocking Safe)
// Does NOT use internal select() to avoid Mac Err 60 / Windows nested select issues.
// Returns -2 on EAGAIN so caller can return to main select loop.
// ==========================================
int recv_body(socket_t sock, void *body, size_t body_size) {
    int total = 0;
    while (total < (int)body_size) {
        int chunk = recv(sock, (char *)body + total, (int)body_size - total, 0);
        if (chunk > 0) {
            total += chunk;
        } else if (chunk == 0) {
            return -1;
        } else {
            int e = GET_SOCKET_ERROR();
            if (e == EAGAIN || e == EWOULDBLOCK) {
                // Return -2 to tell caller (e.g., screens.c) to wait/loop.
                return -2;
            }

            #ifdef _WIN32
                if (e == WSAEINTR) continue;
            #else
                if (e == EINTR) continue;
            #endif

            return -1;
        }
    }
    return 0;
}

// --- Helper: Safe Logging ---
static void add_log_unsafe(NetworkContext *ctx, const char *msg) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char time_buf[10];
    strftime(time_buf, sizeof(time_buf), "%H:%M:%S", tm_info);
    snprintf(ctx->log[ctx->log_index], 64, "[%s] %s", time_buf, msg);
    ctx->log_index = (ctx->log_index + 1) % MAX_NET_LOG;
}

// Public — acquires mutex itself.
void add_log(NetworkContext *ctx, const char *msg) {
    pthread_mutex_lock(&ctx->mutex);
    add_log_unsafe(ctx, msg);
    pthread_mutex_unlock(&ctx->mutex);
}

// --- Helper: Broadcast Lobby State ---
void broadcast_lobby(NetworkContext *ctx) {
    LobbyState state;
    state.is_host_playing = ctx->is_host_playing;
    state.rounds          = ctx->rounds;
    state.loadout         = ctx->loadout;

    if (ctx->is_host_playing) {
        state.count = 1 + ctx->client_sockets_count;
        strncpy(state.names[0], ctx->host_name, 32);
        for (int i = 0; i < ctx->client_sockets_count; i++)
            strncpy(state.names[i + 1], ctx->player_names[i + 1], 32);
    } else {
        state.count = ctx->client_sockets_count;
        for (int i = 0; i < ctx->client_sockets_count; i++)
            strncpy(state.names[i], ctx->player_names[i + 1], 32);
    }

    char buf[1 + sizeof(state)];
    buf[0] = PKT_UPDATE;
    memcpy(buf + 1, &state, sizeof(state));

    for (int i = 0; i < ctx->client_sockets_count; i++) {
        send_all(ctx->clients[i], buf, sizeof(buf));
    }
}

// --- Helper: IP Detection ---
char* get_local_ip(char* buffer, size_t length) {
    if (buffer == NULL || length < 16) return NULL;
    strncpy(buffer, "Unknown", length);

    #ifdef _WIN32
        ULONG outBufLen = sizeof(IP_ADAPTER_INFO);
        PIP_ADAPTER_INFO pAdapterInfo = (IP_ADAPTER_INFO *)malloc(outBufLen);
        if (pAdapterInfo != NULL) {
            if (GetAdaptersInfo(pAdapterInfo, &outBufLen) == ERROR_BUFFER_OVERFLOW) {
                free(pAdapterInfo);
                pAdapterInfo = (IP_ADAPTER_INFO *)malloc(outBufLen);
            }
            if (pAdapterInfo && GetAdaptersInfo(pAdapterInfo, &outBufLen) == NO_ERROR) {
                PIP_ADAPTER_INFO pAdapter = pAdapterInfo;
                while (pAdapter) {
                    IP_ADDR_STRING *pAddr = &pAdapter->IpAddressList;
                    while (pAddr) {
                        if (strcmp(pAddr->IpAddress.String, "0.0.0.0") != 0 &&
                            strcmp(pAddr->IpAddress.String, "127.0.0.1") != 0) {
                            strncpy(buffer, pAddr->IpAddress.String, length);
                            free(pAdapterInfo);
                            return buffer;
                        }
                        pAddr = pAddr->Next;
                    }
                    pAdapter = pAdapter->Next;
                }
            }
            if (pAdapterInfo) free(pAdapterInfo);
        }
    #else
        struct ifaddrs *ifaddr, *ifa;
        if (getifaddrs(&ifaddr) == -1) {
            char hostname[256];
            if (gethostname(hostname, sizeof(hostname)) == 0) {
                struct hostent *he = gethostbyname(hostname);
                if (he != NULL) {
                    struct in_addr addr;
                    memcpy(&addr, he->h_addr_list[0], sizeof(struct in_addr));
                    if (addr.s_addr != inet_addr("127.0.0.1")) {
                        strncpy(buffer, inet_ntoa(addr), length);
                        return buffer;
                    }
                }
            }
            return NULL;
        }
        int found = 0;
        for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
            if (ifa->ifa_addr == NULL) continue;
            if (ifa->ifa_addr->sa_family == AF_INET) {
                struct sockaddr_in *sa = (struct sockaddr_in *)ifa->ifa_addr;
                char ipbuf[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &(sa->sin_addr), ipbuf, INET_ADDRSTRLEN);

                if (strncmp(ipbuf, "127.", 4) == 0) continue;
                if (strncmp(ipbuf, "169.254", 7) == 0) continue;

                const char *name = ifa->ifa_name;
                if (strncmp(name, "lo", 2) == 0) continue;
                if (strncmp(name, "docker", 6) == 0) continue;
                if (strncmp(name, "br-", 3) == 0) continue;
                if (strncmp(name, "veth", 4) == 0) continue;
                if (strncmp(name, "utun", 4) == 0) continue;
                if (strncmp(name, "awdl", 4) == 0) continue;

                strncpy(buffer, ipbuf, length);
                found = 1;
                break;
            }
        }
        freeifaddrs(ifaddr);
        if (found) return buffer;
    #endif
    return NULL;
}

// --- Thread Wrappers ---
#ifdef _WIN32
    DWORD WINAPI server_thread(LPVOID arg) {
#else
    void* server_thread(void* arg) {
#endif
    NetworkContext *ctx = (NetworkContext *)arg;
    struct sockaddr_in server_addr;

    #ifdef _WIN32
        int addrlen = sizeof(struct sockaddr_in);
    #else
        socklen_t addrlen = sizeof(struct sockaddr_in);
    #endif

    ctx->sock = socket(AF_INET, SOCK_STREAM, 0);
    if (ctx->sock == INVALID_SOCK) THREAD_RETURN;

    int opt = 1;
    setsockopt(ctx->sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));

    set_socket_nonblocking(ctx->sock);

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(DEFAULT_PORT);

    if (bind(ctx->sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        CLOSE_SOCKET(ctx->sock);
        ctx->sock = INVALID_SOCK;
        THREAD_RETURN;
    }

    listen(ctx->sock, 5);
    add_log(ctx, "Server started. Waiting for players...");

    while (!ctx->should_stop) {
        if (g_shutdown_requested) break;

        fd_set readfds;
        int max_sd;

        FD_ZERO(&readfds);
        FD_SET(ctx->sock, &readfds);
        max_sd = ctx->sock;

        pthread_mutex_lock(&ctx->mutex);
        // Only add client sockets to select if we are in LOBBY phase
        if (!ctx->game_started) {
            for (int i = 0; i < ctx->client_sockets_count; i++) {
                socket_t sd = ctx->clients[i];
                if (sd > 0) {
                    FD_SET(sd, &readfds);
                    if (sd > max_sd) max_sd = sd;
                }
            }
        }
        pthread_mutex_unlock(&ctx->mutex);

        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        int activity = select(max_sd + 1, &readfds, NULL, NULL, &timeout);
        if (activity < 0) {
            int sel_err = GET_SOCKET_ERROR();
            #ifdef _WIN32
            if (sel_err != WSAEINTR) break;
            #else
            if (sel_err != EINTR) break;
            #endif
        }

        // --- New Connection ---
        if (FD_ISSET(ctx->sock, &readfds)) {
            struct sockaddr_in client_addr_in;
            socket_t new_socket = accept(ctx->sock, (struct sockaddr *)&client_addr_in, &addrlen);

            if (new_socket != INVALID_SOCK) {
                int flag = 1;
                setsockopt(new_socket, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(int));

                set_socket_nonblocking(new_socket);

                pthread_mutex_lock(&ctx->mutex);
                int max_clients = ctx->is_host_playing ? (MAX_PLAYERS - 1) : MAX_PLAYERS;

                if (ctx->game_started) {
                    pthread_mutex_unlock(&ctx->mutex);
                    CLOSE_SOCKET(new_socket);
                } else if (ctx->client_sockets_count >= max_clients) {
                     pthread_mutex_unlock(&ctx->mutex);
                     CLOSE_SOCKET(new_socket);
                } else {
                    ctx->clients[ctx->client_sockets_count] = new_socket;
                    ctx->client_sockets_count++;
                    strncpy(ctx->player_names[ctx->client_sockets_count], "Connecting...", 32);
                    ctx->player_count++;
                    pthread_mutex_unlock(&ctx->mutex);
                }
            }
        }

        // --- Client Activity (Lobby Only) ---
        #define REMOVE_CLIENT_LOCKED(idx) do {                                  \
            for (int _j = (idx); _j < ctx->client_sockets_count - 1; _j++) {   \
                ctx->clients[_j] = ctx->clients[_j + 1];                       \
                ctx->player_names[_j + 1][0] = '\0';                           \
                strncpy(ctx->player_names[_j + 1],                             \
                        ctx->player_names[_j + 2], 32);                        \
            }                                                                   \
            ctx->client_sockets_count--;                                        \
            ctx->player_count--;                                                \
            i--;                                                                \
        } while (0)

        pthread_mutex_lock(&ctx->mutex);
        if (!ctx->game_started) {
            for (int i = 0; i < ctx->client_sockets_count; i++) {
                socket_t sd = ctx->clients[i];
                if (!FD_ISSET(sd, &readfds)) continue;

                char header_byte;
                int valread = recv(sd, &header_byte, 1, 0);

                if (valread > 0) {
                    if (header_byte == PKT_JOIN) {
                        char name_body[32];
                        memset(name_body, 0, sizeof(name_body));
                        pthread_mutex_unlock(&ctx->mutex);

                        // Note: recv_body returns -2 on EAGAIN, handled by loop above
                        int body_res = recv_body(sd, name_body, 31);
                        if (body_res < 0) {
                             char disconnected_name[32];
                             strncpy(disconnected_name, ctx->player_names[i + 1], 31);
                             disconnected_name[31] = '\0';
                             CLOSE_SOCKET(sd);

                             pthread_mutex_lock(&ctx->mutex);
                             for (int k = i; k < ctx->client_sockets_count - 1; k++) {
                                 ctx->clients[k] = ctx->clients[k+1];
                                 ctx->player_names[k+1][0] = '\0';
                                 strncpy(ctx->player_names[k+1], ctx->player_names[k+2], 32);
                             }
                             ctx->client_sockets_count--;
                             ctx->player_count--;

                             char log_msg[64];
                             snprintf(log_msg, 64, "[%s] disconnected (join timeout).", disconnected_name);
                             add_log_unsafe(ctx, log_msg);
                             pthread_mutex_unlock(&ctx->mutex);
                             broadcast_lobby(ctx);
                             pthread_mutex_lock(&ctx->mutex);
                             continue;
                        }

                        pthread_mutex_lock(&ctx->mutex);

                        char incoming_name[32];
                        strncpy(incoming_name, name_body, 31);
                        incoming_name[31] = '\0';

                        if (incoming_name[0] == '\0') {
                            CLOSE_SOCKET(sd);
                            REMOVE_CLIENT_LOCKED(i);
                            add_log_unsafe(ctx, "Empty name rejected.");
                            pthread_mutex_unlock(&ctx->mutex);
                            broadcast_lobby(ctx);
                            pthread_mutex_lock(&ctx->mutex);
                            continue;
                        }

                        int is_duplicate = (strcmp(incoming_name, ctx->host_name) == 0);
                        if (!is_duplicate) {
                            for (int j = 0; j < ctx->client_sockets_count; j++) {
                                if (j == i) continue;
                                if (strcmp(incoming_name, ctx->player_names[j + 1]) == 0) {
                                    is_duplicate = 1;
                                    break;
                                }
                            }
                        }

                        if (is_duplicate) {
                            char log_msg[64];
                            snprintf(log_msg, 64, "Duplicate name '%s' rejected.", incoming_name);
                            CLOSE_SOCKET(sd);
                            REMOVE_CLIENT_LOCKED(i);
                            add_log_unsafe(ctx, log_msg);
                            pthread_mutex_unlock(&ctx->mutex);
                            broadcast_lobby(ctx);
                            pthread_mutex_lock(&ctx->mutex);
                        } else {
                            strncpy(ctx->player_names[i + 1], incoming_name, 32);
                            char msg[64];
                            snprintf(msg, 64, "[%s] connected.", incoming_name);
                            add_log_unsafe(ctx, msg);
                            pthread_mutex_unlock(&ctx->mutex);
                            broadcast_lobby(ctx);
                            pthread_mutex_lock(&ctx->mutex);
                        }
                    }
                } else if (valread == 0) {
                    char disconnected_name[32];
                    strncpy(disconnected_name, ctx->player_names[i + 1], 31);
                    disconnected_name[31] = '\0';
                    CLOSE_SOCKET(sd);
                    REMOVE_CLIENT_LOCKED(i);
                    char log_msg[64];
                    snprintf(log_msg, 64, "[%s] disconnected.", disconnected_name);
                    add_log_unsafe(ctx, log_msg);
                    pthread_mutex_unlock(&ctx->mutex);
                    broadcast_lobby(ctx);
                    pthread_mutex_lock(&ctx->mutex);
                } else {
                    int err = GET_SOCKET_ERROR();
                    if (err == EAGAIN || err == EWOULDBLOCK) continue;
                    char disconnected_name[32];
                    strncpy(disconnected_name, ctx->player_names[i + 1], 31);
                    disconnected_name[31] = '\0';
                    CLOSE_SOCKET(sd);
                    REMOVE_CLIENT_LOCKED(i);
                    char log_msg[64];
                    snprintf(log_msg, 64, "[%s] disconnected (Error: %d).", disconnected_name, err);
                    add_log_unsafe(ctx, log_msg);
                    pthread_mutex_unlock(&ctx->mutex);
                    broadcast_lobby(ctx);
                    pthread_mutex_lock(&ctx->mutex);
                }
            }
        }
        pthread_mutex_unlock(&ctx->mutex);
        #undef REMOVE_CLIENT_LOCKED
    }

    for (int i = 0; i < ctx->client_sockets_count; i++) {
        CLOSE_SOCKET(ctx->clients[i]);
    }
    CLOSE_SOCKET(ctx->sock);
    ctx->sock = INVALID_SOCK;
    THREAD_RETURN;
}

#ifdef _WIN32
    DWORD WINAPI client_thread(LPVOID arg) {
#else
    void* client_thread(void* arg) {
#endif
    NetworkContext *ctx = (NetworkContext *)arg;
    struct sockaddr_in server_addr;

    // Create Socket
    ctx->sock = socket(AF_INET, SOCK_STREAM, 0);
    if (ctx->sock == INVALID_SOCK) {
        ctx->is_connected = false;
        THREAD_RETURN;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(DEFAULT_PORT);
    if (inet_pton(AF_INET, ctx->remote_ip, &server_addr.sin_addr) <= 0) {
        CLOSE_SOCKET(ctx->sock);
        ctx->sock = INVALID_SOCK;
        ctx->is_connected = false;
        THREAD_RETURN;
    }

    int flag = 1;
    setsockopt(ctx->sock, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(int));

    if (connect(ctx->sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        CLOSE_SOCKET(ctx->sock);
        ctx->sock = INVALID_SOCK;
        ctx->is_connected = false;
        THREAD_RETURN;
    }

    {
        char buf[32];
        memset(buf, 0, sizeof(buf));
        buf[0] = PKT_JOIN;
        strncpy(buf + 1, ctx->host_name, 31);

        if (send_all(ctx->sock, buf, 32) < 0) {
            CLOSE_SOCKET(ctx->sock);
            ctx->sock = INVALID_SOCK;
            ctx->is_connected = false;
            THREAD_RETURN;
        }
    }

    ctx->is_connected = true;

    // Lobby Loop
    while (!ctx->should_stop) {
        char header;

        if (recv_blocking(ctx->sock, &header, 1) < 0) {
            goto disconnect_client;
        }

        if (header == PKT_START) {
            pthread_mutex_lock(&ctx->mutex);
            ctx->game_started = true;

            int count = 0;
            for(int i=0; i<MAX_PLAYERS; i++) {
                if(ctx->player_names[i][0] != '\0') count++;
            }
            ctx->player_count = count;
            pthread_mutex_unlock(&ctx->mutex);

            break;
        }

        if (header == PKT_UPDATE) {
            LobbyState state;
            if (recv_blocking(ctx->sock, &state, sizeof(state)) < 0) goto disconnect_client;

            pthread_mutex_lock(&ctx->mutex);
            ctx->player_count     = state.count;
            ctx->is_host_playing  = state.is_host_playing;
            ctx->rounds           = state.rounds;
            ctx->loadout          = state.loadout;
            for (int i = 0; i < state.count && i < MAX_PLAYERS; i++)
                strncpy(ctx->player_names[i], state.names[i], 32);
            pthread_mutex_unlock(&ctx->mutex);
        }

        if (header == PKT_LOG) {
            LogPacket lp;
            if (recv_blocking(ctx->sock, &lp, sizeof(lp)) < 0) goto disconnect_client;
            pthread_mutex_lock(&ctx->mutex);
            add_log_unsafe(ctx, lp.msg);
            pthread_mutex_unlock(&ctx->mutex);
        }
    }

    // Screens.c needs non-blocking behavior.
    set_socket_nonblocking(ctx->sock);

    // Sleep until disconnect (Screens.c is doing the heavy lifting)
    while (!ctx->should_stop) {
        #ifdef _WIN32
            Sleep(100);
        #else
            usleep(100000);
        #endif
    }

    disconnect_client:
        CLOSE_SOCKET(ctx->sock);
        ctx->sock = INVALID_SOCK;
        ctx->is_connected = false;
    THREAD_RETURN;
}

// --- Public API ---
void network_broadcast_start(NetworkContext *ctx) {
    char start_signal = PKT_START;
    if (ctx->is_host) {
        pthread_mutex_lock(&ctx->mutex);
        for (int i = 0; i < ctx->client_sockets_count; i++) {
            send_all(ctx->clients[i], &start_signal, 1);
        }
        ctx->game_started = true;
        pthread_mutex_unlock(&ctx->mutex);
    }
}

int network_start_server(NetworkContext *ctx) {
    memset(ctx, 0, sizeof(NetworkContext));
    ctx->is_host = true;
    ctx->should_stop = false;
    pthread_mutex_init(&ctx->mutex, NULL);
    ctx->player_count = ctx->is_host_playing ? 1 : 0;

    g_server_ctx = ctx;
    g_shutdown_requested = 0;
    #ifndef _WIN32
        signal(SIGINT, signal_handler);
        signal(SIGTERM, signal_handler);
    #endif
#ifdef _WIN32
    HANDLE thread = CreateThread(NULL, 0, server_thread, ctx, 0, NULL);
    if (thread == NULL) return -1;
    CloseHandle(thread);
#else
    pthread_t thread;
    if (pthread_create(&thread, NULL, server_thread, ctx) != 0) return -1;
    pthread_detach(thread);
#endif
    return 0;
}

int network_start_client(NetworkContext *ctx, const char *ip, const char *name) {
    memset(ctx, 0, sizeof(NetworkContext));
    ctx->is_host = false;
    ctx->should_stop = false;
    strncpy(ctx->remote_ip, ip, sizeof(ctx->remote_ip) - 1);
    pthread_mutex_init(&ctx->mutex, NULL);
    if (name != NULL) strncpy(ctx->host_name, name, sizeof(ctx->host_name));

#ifdef _WIN32
    HANDLE thread = CreateThread(NULL, 0, client_thread, ctx, 0, NULL);
    if (thread == NULL) return -1;
    CloseHandle(thread);
#else
    pthread_t thread;
    if (pthread_create(&thread, NULL, client_thread, ctx) != 0) return -1;
    pthread_detach(thread);
#endif
    return 0;
}

void network_disconnect(NetworkContext *ctx) {
    ctx->should_stop = true;
    g_shutdown_requested = 1;
    g_server_ctx = NULL;
    pthread_mutex_lock(&ctx->mutex);
    if (ctx->sock != INVALID_SOCK) {
#ifdef _WIN32
        shutdown(ctx->sock, SD_BOTH);
#endif
        CLOSE_SOCKET(ctx->sock);
        ctx->sock = INVALID_SOCK;
    }
    for (int i = 0; i < ctx->client_sockets_count; i++) {
        if (ctx->clients[i] != INVALID_SOCK) CLOSE_SOCKET(ctx->clients[i]);
    }
    ctx->client_sockets_count = 0;
    ctx->is_connected = false;
    pthread_mutex_unlock(&ctx->mutex);
    pthread_mutex_destroy(&ctx->mutex);
}

// --- Game Networking Implementation ---

void send_sync(NetworkContext *ctx, GameState *gs) {
        SyncPacket sp;
        sp.n_shells = gs->n_shells;
        sp.live_count = gs->live_count;
        sp.blank_count = gs->blank_count;
        sp.current_shell = gs->current_shell;
        sp.round = gs->round;
        sp.current_player_index = gs->current_player_index;
        sp.show_shells = gs->show_shells;
        sp.cuffs_target = gs->cuffs_target;

        memcpy(sp.players, gs->players, sizeof(gs->players));
        memcpy(sp.visual_map, gs->visual_map, sizeof(gs->visual_map));

        // Mask every shell as UNKNOWN — clients must not know the actual sequence.
        // Private shell reveals (Magnifying Glass, Burner Phone) are sent via
        // targeted PKT_LOG messages, not through the sync packet.
        for (int i = 0; i < MAX_SHELLS; i++) {
            sp.shells[i] = SHELL_UNKNOWN;
        }

        char buf[sizeof(sp) + 1];
        buf[0] = PKT_SYNC;
        memcpy(buf + 1, &sp, sizeof(sp));

        gs->show_shells = 0;

        pthread_mutex_lock(&ctx->mutex);
        for (int i = 0; i < ctx->client_sockets_count; i++) {
            send_all(ctx->clients[i], buf, sizeof(buf));
        }
        pthread_mutex_unlock(&ctx->mutex);
    }

void broadcast_action(NetworkContext *ctx, ActionPacket *ap) {
    char buf[sizeof(*ap) + 1];
    buf[0] = PKT_ACTION;
    memcpy(buf + 1, ap, sizeof(*ap));

    pthread_mutex_lock(&ctx->mutex);
    for (int i = 0; i < ctx->client_sockets_count; i++) {
        send_all(ctx->clients[i], buf, sizeof(buf));
    }
    pthread_mutex_unlock(&ctx->mutex);
}


// --- Helper to send log messages ---
void broadcast_log(NetworkContext *ctx, const char *msg) {
    LogPacket lp;
    strncpy(lp.msg, msg, 63);
    lp.msg[63] = '\0';

    char buf[sizeof(lp) + 1];
    buf[0] = PKT_LOG;
    memcpy(buf + 1, &lp, sizeof(lp));

    pthread_mutex_lock(&ctx->mutex);
    for (int i = 0; i < ctx->client_sockets_count; i++) {
        send_all(ctx->clients[i], buf, sizeof(buf));
    }
    pthread_mutex_unlock(&ctx->mutex);
}

// --- Send a log message to one specific player ---
void send_log_to_player(NetworkContext *ctx, int player_idx, const char *msg) {
        // If the target is the host (player 0) and host is playing, do nothing –
        // the host sees the log locally.
        if (ctx->is_host_playing && player_idx == 0) {
            return;
        }

        int client_slot;
        if (ctx->is_host_playing) {
            // player 1 → slot 0, player 2 → slot 1, …
            client_slot = player_idx - 1;
        } else {
            // player 0 → slot 0, player 1 → slot 1, …
            client_slot = player_idx;
        }

        if (client_slot < 0 || client_slot >= ctx->client_sockets_count) {
            return; // invalid or disconnected player
        }

        LogPacket lp;
        strncpy(lp.msg, msg, 63);
        lp.msg[63] = '\0';

        char buf[sizeof(lp) + 1];
        buf[0] = PKT_LOG;
        memcpy(buf + 1, &lp, sizeof(lp));

        pthread_mutex_lock(&ctx->mutex);
        if (client_slot < ctx->client_sockets_count) {
            send_all(ctx->clients[client_slot], buf, sizeof(buf));
        }
        pthread_mutex_unlock(&ctx->mutex);
    }

// --- send_action_to_host ---
void send_action_to_host(NetworkContext *ctx, GameState *gs, ActionPacket *ap) {
    char packet[sizeof(ActionPacket) + 1];
    packet[0] = PKT_ACTION;
    memcpy(packet + 1, ap, sizeof(ActionPacket));

    int total_len = 1 + sizeof(ActionPacket);
    if (send_all(ctx->sock, packet, total_len) < 0) {
        if (gs && g_debug_mode) {
            char err[64];
            snprintf(err, 64, "[NET ERROR] Send failed! Code: %d", GET_SOCKET_ERROR());
            game_log(gs, err);
        }
    }
}

// --- recv_packet ---
int recv_packet(socket_t sock, char *header, void *body, size_t body_size, GameState *gs) {
    // 1. Read Header
    int r = recv(sock, header, 1, 0);

    if (r == 0) return -1;

    if (r < 0) {
        int err_code = GET_SOCKET_ERROR();
        if (err_code == EAGAIN || err_code == EWOULDBLOCK) return -2;
        if (gs) {
            char err_msg[64];
            snprintf(err_msg, 64, "Socket Hdr Err: %d", err_code);
            game_log(gs, err_msg);
        }
        return -1;
    }

    // 2. Read Body
    if (body_size > 0) {
        if (recv_body(sock, body, body_size) < 0) {
            if (gs) game_log(gs, "Socket Body: timeout or disconnect.");
            return -1;
        }
    }
    return 0;
}

void send_order(NetworkContext *ctx, GameState *gs) {
        OrderPacket op;
        op.n_shells = gs->n_shells;
        op.round = gs->round;
        memcpy(op.shells, gs->shells, sizeof(gs->shells));

        char buf[sizeof(op) + 1];
        buf[0] = PKT_ORDER;
        memcpy(buf + 1, &op, sizeof(op));

        pthread_mutex_lock(&ctx->mutex);
        for (int i = 0; i < ctx->client_sockets_count; i++) {
            send_all(ctx->clients[i], buf, sizeof(buf));
        }
        pthread_mutex_unlock(&ctx->mutex);
    }

int recv_order(socket_t sock, char *header, OrderPacket *op) {
    int r = recv(sock, header, 1, 0);
    if (r <= 0) return -1;
    return recv_body(sock, op, sizeof(*op));
}