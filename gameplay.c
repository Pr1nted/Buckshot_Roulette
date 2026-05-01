#include "gameplay.h"
#include "ui.h"

void advance_turn(GameState *gs) {
    int start_index = gs->current_player_index;

    gs->current_player_index = (gs->current_player_index + 1) % gs->player_count;

    while (gs->players[gs->current_player_index].charges <= 0) {
        if (gs->current_player_index == start_index) break;
        gs->current_player_index = (gs->current_player_index + 1) % gs->player_count;
    }

    gs->saw_active = 0;
}

int RangeRand(int min, int max) {
    return rand() % (max - min + 1) + min;
}

void game_log(GameState *gs, const char *msg) {
    if (gs->suppress_log) return;
    if (!gs->is_host && strncmp(msg, "[PRIVATE] ", 10) == 0) msg += 10;

    int idx = gs->n_log % MAX_LOG;
    strncpy(gs->log[idx], msg, 63);
    gs->log[idx][63] = '\0';
    gs->n_log++;
}

void generate_shells(GameState *gs) {
    int total = RangeRand(2, MAX_SHELLS);

    int live  = RangeRand(1, total - 1);
    int blank = total - live;

    if (live > blank) {
        int tmp = live;
        live    = blank;
        blank   = tmp;
    }

    gs->n_shells    = total;
    gs->live_count  = live;
    gs->blank_count = blank;

    for (int i = 0; i < live; i++)
        gs->shells[i] = SHELL_LIVE;
    for (int i = live; i < total; i++)
        gs->shells[i] = SHELL_BLANK;

    for (int i = total - 1; i > 0; i--) {
        int j = RangeRand(0, i);
        ShellType tmp   = gs->shells[i];
        gs->shells[i]   = gs->shells[j];
        gs->shells[j]   = tmp;
    }

    gs->current_shell = 0;
}

void give_new_items(GameState *gs) {
    int n_to_add = RangeRand(2, 4);

    ItemType classic_pool[] = {
        ITEM_BEER, ITEM_CIGARETTE, ITEM_HANDSAW,
        ITEM_HANDCUFFS, ITEM_MAGNIFYING_GLASS
    };
    ItemType double_pool[] = {
        ITEM_BEER, ITEM_CIGARETTE, ITEM_HANDSAW, ITEM_HANDCUFFS,
        ITEM_MAGNIFYING_GLASS, ITEM_ADRENALINE, ITEM_EXPIRED_MEDICINE,
        ITEM_INVERTER, ITEM_BURNER_PHONE
    };

    ItemType *pool = (active_loadout == 1) ? double_pool : classic_pool;
    int pool_size = (active_loadout == 1) ? 9 : 5;

    for (int p = 0; p < gs->player_count; p++) {
        Player *current_p = &gs->players[p];
        int added_this_round = 0;

        for (int i = 0; i < n_to_add; i++) {
            for (int slot = 0; slot < MAX_ITEMS; slot++) {
                if (current_p->items[slot] == ITEM_NONE) {
                    current_p->items[slot] = pool[RangeRand(0, pool_size - 1)];
                    added_this_round++;
                    break;
                }
            }
        }
    }
    game_log(gs, "New items dealt to all players.");
}


void perform_multiplayer_shoot(GameState *gs, int shooter_idx, int target_idx, WINDOW *gwin) {
    ShellType current = gs->shells[gs->current_shell];
    int damage = gs->saw_active ? 2 : 1;
    const char *shooter_name = gs->players[shooter_idx].name;
    const char *target_name = gs->players[target_idx].name;

    char msg[128];
    if (shooter_idx == target_idx) {
        snprintf(msg, 128, "%s shoots THEMSELVES", shooter_name);
    } else {
        snprintf(msg, 128, "%s aims at %s...", shooter_name, target_name);
    }
    game_log(gs, msg);

    int turn_swaps = 1;
    if (current == SHELL_LIVE) {
        gs->players[target_idx].charges -= damage;
        game_log(gs, "BOOM! Live round.");
        if (gs->players[target_idx].charges < 0) gs->players[target_idx].charges = 0;
        if (gwin) trigger_flash(gwin);
    } else {
        game_log(gs, "Click. It was a blank.");
        if (shooter_idx == target_idx) turn_swaps = 0;
    }

    gs->current_shell++;
    gs->saw_active = 0;

    if (turn_swaps) {
        if (gs->cuffs_target >= 0) {
            int next = (gs->current_player_index + 1) % gs->player_count;
            if (next == gs->cuffs_target) {
                game_log(gs, "Target is handcuffed! Turn skipped.");
                gs->current_player_index = next;
            }
            gs->cuffs_target = -1;
        }

        gs->current_player_index = (gs->current_player_index + 1) % gs->player_count;

        // SKIP DEAD PLAYERS
        int safety_count = 0;
        while (gs->players[gs->current_player_index].charges <= 0 && safety_count < gs->player_count) {
            gs->current_player_index = (gs->current_player_index + 1) % gs->player_count;
            safety_count++;
        }
    }
}

void perform_steal(GameState *gs, int actor_idx, int target_idx, int steal_slot) {
    Player *current_p = &gs->players[actor_idx];
    Player *victim_p = &gs->players[target_idx];

    if (steal_slot >= 0 && steal_slot < MAX_ITEMS) {
        ItemType stolen_item = victim_p->items[steal_slot];

        if (stolen_item != ITEM_NONE && stolen_item != ITEM_ADRENALINE) {
            // Remove from victim
            for (int i = steal_slot; i < MAX_ITEMS - 1; i++)
                victim_p->items[i] = victim_p->items[i + 1];
            victim_p->items[MAX_ITEMS - 1] = ITEM_NONE;

            int found_slot = -1;
            for (int i = 0; i < MAX_ITEMS; i++) {
                if (current_p->items[i] == ITEM_NONE) {
                    found_slot = i;
                    break;
                }
            }

            if (found_slot != -1) {
                current_p->items[found_slot] = stolen_item;
                char msg[64];
                snprintf(msg, sizeof(msg), "%s stole %s!",
                         current_p->name, item_name(stolen_item));
                game_log(gs, msg);
            } else {
                game_log(gs, "Inventory full!");
            }
        } else {
            game_log(gs, "Nothing stealable there.");
        }
    }

    for (int i = 0; i < MAX_ITEMS; i++) {
        if (current_p->items[i] == ITEM_ADRENALINE) {
            for (int j = i; j < MAX_ITEMS - 1; j++)
                current_p->items[j] = current_p->items[j + 1];
            current_p->items[MAX_ITEMS - 1] = ITEM_NONE;
            break;
        }
    }

    gs->adrenaline_active = 0;
    gs->selected_item = 0;
}

void use_item(GameState *gs, int item_idx, int actor_idx, int target_idx) {
    int other_idx = -1;
    for(int i=0; i<gs->player_count; i++) {
        if(i != actor_idx) {
            other_idx = i;
            break;
        }
    }
    if (other_idx == -1) other_idx = 0;

    Player *current_p = &gs->players[actor_idx];

    Player *other_p   = &gs->players[other_idx];
    ItemType item = current_p->items[item_idx];

    if (item == ITEM_NONE) return;

    char msg[128];

    int should_log = (gs->is_host || gs->player_count <= 1);

    if (should_log) {
        snprintf(msg, sizeof(msg), "%s uses %s.", gs->players[actor_idx].name, item_name(item));
        game_log(gs, msg);
    }

    switch (item) {
        case ITEM_BEER:
            if (gs->current_shell < gs->n_shells) {
                ShellType ejected = gs->shells[gs->current_shell];
                game_log(gs, ejected == SHELL_LIVE ? "Ejected a LIVE round." : "Ejected a BLANK.");
                if (ejected == SHELL_LIVE) gs->live_count--;
                else gs->blank_count--;
                gs->current_shell++;
                gs->shells_ejected++;
                gs->beer_ml += 330;
            }
            break;

        case ITEM_CIGARETTE:
            if (current_p->charges < current_p->max_charges) {
                current_p->charges++;
                game_log(gs, "Gained 1 charge.");
                gs->cigarettes_smoked++;
            } else {
                game_log(gs, "Already at max charges!");
            }
            break;

        case ITEM_HANDSAW:
            if (!gs->saw_active) {
                gs->saw_active = 1;
                game_log(gs, "Shotgun sawn off! Double damage.");
            }
            break;

        case ITEM_HANDCUFFS: {
                // Use the explicit target_idx passed in (from ActionPacket.target_idx).
                // Fall back to next player only if caller passed -1.
                int cuff_target = target_idx;
                if (cuff_target < 0 || cuff_target >= gs->player_count) {
                    if (gs->player_count == 2) {
                        cuff_target = gs->player_turn ? 1 : 0;
                    } else {
                        cuff_target = (gs->current_player_index + 1) % gs->player_count;
                    }
                }

                gs->cuffs_target = cuff_target;

                char cuffs_msg[64];
                snprintf(cuffs_msg, 64, "%s cuffed %s!",
                         gs->players[actor_idx].name,
                         gs->players[cuff_target].name);
                game_log(gs, cuffs_msg);
            break;
        }

        case ITEM_MAGNIFYING_GLASS:
            if (gs->current_shell < gs->n_shells) {
                // Only the host generates the reveal; clients receive it via send_log_to_player
                if (gs->is_host || gs->player_count <= 1) {
                    ShellType current_shell_type = gs->shells[gs->current_shell];
                    snprintf(msg, sizeof(msg), "[PRIVATE] It's a %s.", current_shell_type == SHELL_LIVE ? "LIVE round" : "BLANK");
                    game_log(gs, msg);
                }
            }
            break;


        case ITEM_EXPIRED_MEDICINE:
            if (RangeRand(0, 1)) {
                current_p->charges = (current_p->charges + 2 > current_p->max_charges) ?
                                      current_p->max_charges : current_p->charges + 2;
                game_log(gs, "Medicine worked! +2 charges.");
            } else {
                current_p->charges--;
                game_log(gs, "Medicine was expired... -1 charge.");

                // If the player kills themselves with medicine, pass the turn
                if (current_p->charges <= 0) {
                    game_log(gs, "Player eliminated themselves!");
                    advance_turn(gs);
                }
            }
            break;

        case ITEM_INVERTER:
            if (gs->current_shell < gs->n_shells) {
                if (gs->shells[gs->current_shell] == SHELL_LIVE) {
                    gs->shells[gs->current_shell] = SHELL_BLANK;
                    gs->live_count--; gs->blank_count++;
                } else {
                    gs->shells[gs->current_shell] = SHELL_LIVE;
                    gs->blank_count--; gs->live_count++;
                }
                game_log(gs, "Current shell inverted.");
            }
            break;

        case ITEM_BURNER_PHONE:
            if (gs->current_shell < gs->n_shells - 1) {
                if (gs->is_host || gs->player_count <= 1) {
                    int future_idx = RangeRand(gs->current_shell + 1, gs->n_shells - 1);
                    int relative_pos = future_idx - gs->current_shell; // 1 = next shell, 2 = two away, etc.
                    snprintf(msg, sizeof(msg), "[PRIVATE] Shell +%d is %s.", relative_pos,
                             gs->shells[future_idx] == SHELL_LIVE ? "LIVE" : "BLANK");
                    game_log(gs, msg);
                }
            }
            break;

        case ITEM_ADRENALINE:
            // Code below is unused, this is part of old logic as far as I remember

            /*
            // --- EXECUTE STEAL PHASE ---
            if (gs->adrenaline_active) {
                int target_player = (target_idx >= 0 && target_idx < gs->player_count)
                                    ? target_idx : other_idx;
                int steal_slot = item_idx;

                Player *victim_p = &gs->players[target_player];

                if (steal_slot >= 0 && steal_slot < MAX_ITEMS) {
                    ItemType stolen_item = victim_p->items[steal_slot];

                    if (stolen_item != ITEM_NONE && stolen_item != ITEM_ADRENALINE) {
                        for (int i = steal_slot; i < MAX_ITEMS - 1; i++)
                            victim_p->items[i] = victim_p->items[i + 1];
                        victim_p->items[MAX_ITEMS - 1] = ITEM_NONE;

                        int found_slot = -1;
                        for (int i = 0; i < MAX_ITEMS; i++) {
                            if (current_p->items[i] == ITEM_NONE) {
                                found_slot = i;
                                break;
                            }
                        }

                        if (found_slot != -1) {
                            current_p->items[found_slot] = stolen_item;
                            char msg[64];
                            snprintf(msg, sizeof(msg), "%s stole %s!",
                                     gs->players[actor_idx].name, item_name(stolen_item));
                            game_log(gs, msg);
                        } else {
                            game_log(gs, "Inventory full!");
                        }
                    } else {
                        game_log(gs, "Nothing stealable there.");
                    }
                }

                for (int i = 0; i < MAX_ITEMS; i++) {
                    if (current_p->items[i] == ITEM_ADRENALINE) {
                        for (int j = i; j < MAX_ITEMS - 1; j++)
                            current_p->items[j] = current_p->items[j + 1];
                        current_p->items[MAX_ITEMS - 1] = ITEM_NONE;
                        break;
                    }
                }

                gs->adrenaline_active = 0;
                gs->selected_item = 0;
                return;
            }*/
            break;
        default: break;
    }

    // Remove the used item from inventory
    for (int i = item_idx; i < MAX_ITEMS - 1; i++)
        current_p->items[i] = current_p->items[i + 1];
    current_p->items[MAX_ITEMS - 1] = ITEM_NONE;

    if (gs->current_shell >= gs->n_shells) {
        generate_shells(gs);
        give_new_items(gs);
        char reload_msg[64];
        snprintf(reload_msg, sizeof(reload_msg),
                 "Reloaded: %d live, %d blank.", gs->live_count, gs->blank_count);
        game_log(gs, reload_msg);
        gs->show_shells = 1;
        gs->shell_reveal_time = time(NULL);
    }
}

// --- Offline 1v1 Functions ---

void fire_shotgun(GameState *gs, int target_is_dealer, WINDOW *gwin) {
    // Map 1v1 logic to Array Indices
    int shooter_idx = gs->player_turn ? 0 : 1;
    int target_idx  = target_is_dealer ? 1 : 0;

    ShellType current = gs->shells[gs->current_shell];
    int damage = gs->saw_active ? 2 : 1;
    int shooter_was_player = (shooter_idx == 0);

    if (shooter_was_player) {
        if (target_is_dealer) game_log(gs, "You aim at DEALER...");
        else game_log(gs, "You aim at YOURSELF...");
    } else {
        if (target_is_dealer) game_log(gs, "Dealer aims at HIMSELF...");
        else game_log(gs, "Dealer aims at YOU...");
    }

    int should_swap_turn = 1;

    if (current == SHELL_LIVE) {
        game_log(gs, "BANG! It was LIVE.");
        gs->players[target_idx].charges -= damage;
        gs->damage_dealt += damage;
        gs->money += (2000 * damage);

        gs->live_count--;
        should_swap_turn = 1;
        if (gwin) trigger_flash(gwin);
    } else {
        game_log(gs, "click... It was a BLANK.");
        gs->blank_count--;

        int shot_self = (shooter_idx == target_idx);

        if (shot_self) {
            game_log(gs, shooter_was_player ? "Extra turn granted!" : "Dealer keeps his turn!");
            should_swap_turn = 0;
        } else {
            should_swap_turn = 1;
        }
    }

    if (should_swap_turn) {
        if (gs->cuffs_target >= 0) {
            game_log(gs, "Handcuffs active! Turn skipped.");
            gs->cuffs_target = -1;
        } else {
            gs->player_turn = !gs->player_turn;
        }
    }

    gs->current_shell++;
    gs->saw_active = 0;

    if (gs->players[0].charges <= 0 || gs->players[1].charges <= 0) {
        game_log(gs, "MATCH OVER.");
    } else if (gs->current_shell >= gs->n_shells) {
        game_log(gs, "Gun empty. Reloading...");
        generate_shells(gs);
        give_new_items(gs);

        gs->player_turn = 1;
        gs->cuffs_target = -1;

        gs->show_shells = 1;
        gs->shell_reveal_time = time(NULL);
    }
}

void dealer_ai_turn(GameState *gs, WINDOW *gwin) {
    Player *dealer = &gs->players[1];
    Player *player = &gs->players[0];

    int decided_to_shoot = 0;
    int known_shell = SHELL_UNKNOWN;

    game_log(gs, "Dealer is thinking...");
    if (gwin) { draw_main_ui(gwin, gs); napms(900); }

    while (!decided_to_shoot && dealer->charges > 0 && player->charges > 0) {
        int item_to_use = -1;

        // Healing
        for (int i = 0; i < MAX_ITEMS; i++) {
            if (dealer->items[i] == ITEM_CIGARETTE && dealer->charges < dealer->max_charges) {
                item_to_use = i;
                break;
            }
        }

        // Knowledge
        if (item_to_use == -1 && known_shell == SHELL_UNKNOWN) {
            for (int i = 0; i < MAX_ITEMS; i++) {
                if (dealer->items[i] == ITEM_MAGNIFYING_GLASS) {
                    item_to_use = i;
                    // Read shell directly — don't call use_item which would log
                    // "[PRIVATE] It's a..." and reveal the shell to the player.
                    known_shell = gs->shells[gs->current_shell];
                    break;
                }
            }
        }

        // Offensive
        if (item_to_use == -1) {
            if (known_shell == SHELL_LIVE) {
                for (int i = 0; i < MAX_ITEMS; i++) {
                    if (dealer->items[i] == ITEM_HANDSAW && !gs->saw_active) {
                        item_to_use = i;
                        break;
                    }
                }
            }
        }

        if (item_to_use != -1) {
            // Magnifying glass is consumed silently — use_item would log the
            // private shell reveal and show it to the player.
            if (dealer->items[item_to_use] == ITEM_MAGNIFYING_GLASS) {
                game_log(gs, "Dealer uses Mag.Glass.");
                for (int i = item_to_use; i < MAX_ITEMS - 1; i++)
                    dealer->items[i] = dealer->items[i + 1];
                dealer->items[MAX_ITEMS - 1] = ITEM_NONE;
            } else {
                use_item(gs, item_to_use, 1, -1);
            }
            if (gwin) { draw_main_ui(gwin, gs); napms(900); }
        } else {
            decided_to_shoot = 1;
        }
    }

    if (dealer->charges > 0 && player->charges > 0) {
        if (gwin) { draw_main_ui(gwin, gs); napms(700); }

        if (known_shell == SHELL_LIVE) fire_shotgun(gs, 0, gwin); // Shoot player
        else if (known_shell == SHELL_BLANK) fire_shotgun(gs, 1, gwin); // Shoot self
        else {
            if (gs->live_count >= gs->blank_count) fire_shotgun(gs, 0, gwin); // Assume Live
            else fire_shotgun(gs, 1, gwin); // Assume Blank
        }

        if (gwin) { draw_main_ui(gwin, gs); napms(800); }
    }
}

void init_game(GameState *gs) {
    memset(gs, 0, sizeof(GameState));
    gs->cuffs_target = -1;

    gs->roundAmount = 3;
    int max_charges = RangeRand(2, 4);
    int n_initial_items = RangeRand(2, 4);

    // Initialize Player (Index 0)
    strncpy(gs->players[0].name, "You", 31);
    gs->players[0].max_charges = max_charges;
    gs->players[0].charges = max_charges;

    // Initialize Dealer (Index 1)
    strncpy(gs->players[1].name, "Dealer", 31);
    gs->players[1].max_charges = max_charges;
    gs->players[1].charges = max_charges;

    // Setup indices for offline mode
    gs->player_count = 2;
    gs->local_player_index = 0;
    gs->current_player_index = 0;
    gs->is_host = 1;
    gs->visual_map[POS_BOTTOM] = 0;
    gs->visual_map[POS_TOP] = 1;

    generate_shells(gs);
    give_new_items(gs);

    gs->round = 1;
    gs->player_turn = 1;
    gs->show_shells = 1;
    gs->shell_reveal_time = time(NULL);

    char msg[64];
    snprintf(msg, sizeof(msg), "Round 1: %d Charges, %d Shells.", max_charges, gs->n_shells);
    game_log(gs, "Welcome to the table.");
    game_log(gs, msg);
}


void start_next_round(GameState *gs) {
    int max_charges = RangeRand(2, 4);

    for (int p = 0; p < gs->player_count; p++) {
        Player *current_p = &gs->players[p];

        current_p->max_charges = max_charges;
        current_p->charges = max_charges;

        for (int i = 0; i < MAX_ITEMS; i++) {
            current_p->items[i] = ITEM_NONE;
        }
    }

    // Reset game state flags
    gs->saw_active        = 0;
    gs->cuffs_target      = -1;
    gs->adrenaline_active = 0;
    gs->player_turn       = 1;
    gs->selected_action   = 0;
    gs->selected_item     = 0;
    gs->current_player_index = 0;

    generate_shells(gs);
    give_new_items(gs);

    gs->show_shells       = 1;
    gs->shell_reveal_time = time(NULL);

    gs->round++;

    char msg[64];
    snprintf(msg, sizeof(msg), "Round %d: %d charges, %d shells.",
             gs->round, max_charges, gs->n_shells);
    game_log(gs, msg);
}