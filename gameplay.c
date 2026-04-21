#include "types.h"
#include "ui.h"
#include "gameplay.h"

int RangeRand(int min, int max) {
    return rand() % (max - min + 1) + min;
}

void game_log(GameState *gs, const char *msg) {
    if (gs->n_log < MAX_LOG) {
        strncpy(gs->log[gs->n_log++], msg, 63);
    } else {
        // Scroll log up
        for (int i = 0; i < MAX_LOG - 1; i++)
            strncpy(gs->log[i], gs->log[i + 1], 63);
        strncpy(gs->log[MAX_LOG - 1], msg, 63);
    }
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

    Player *players[2] = {&gs->player, &gs->dealer};

    for (int p = 0; p < 2; p++) {
        int added_this_round = 0;
        for (int i = 0; i < n_to_add; i++) {
            for (int slot = 0; slot < MAX_ITEMS; slot++) {
                if (players[p]->items[slot] == ITEM_NONE) {
                    players[p]->items[slot] = pool[RangeRand(0, pool_size - 1)];
                    added_this_round++;
                    break;
                }
            }
        }
    }
    game_log(gs, "New items dealt to both sides.");
}

void fire_shotgun(GameState *gs, int target_is_dealer, WINDOW *gwin) {
    ShellType current = gs->shells[gs->current_shell];
    int damage = gs->saw_active ? 2 : 1;
    int shooter_was_player = gs->player_turn;

    if (shooter_was_player) {
        if (target_is_dealer) game_log(gs, "You aim at the DEALER...");
        else game_log(gs, "You aim at YOURSELF...");
    } else {
        if (target_is_dealer) game_log(gs, "Dealer aims at HIMSELF...");
        else game_log(gs, "Dealer aims at YOU...");
    }

    int should_swap_turn = 1;

    if (current == SHELL_LIVE) {
        game_log(gs, "BANG! It was LIVE.");
        if (target_is_dealer) {
            gs->dealer.charges -= damage;
            gs->damage_dealt += damage;
            gs->money += (2000 * damage); // Bonus money for damage
        } else {
            gs->player.charges -= damage;
            // FLASH EFFECT
            trigger_flash(gwin);
        }

        gs->live_count--;
        should_swap_turn = 1; // Live rounds always result in a turn swap
    }
    else {
        game_log(gs, "click... It was a BLANK.");
        gs->blank_count--;

        int shot_self = (shooter_was_player && !target_is_dealer) || (!shooter_was_player && target_is_dealer);

        if (shot_self) {
            game_log(gs, shooter_was_player ? "Extra turn granted!" : "Dealer keeps his turn!");
            should_swap_turn = 0;
        } else {
            should_swap_turn = 1;
        }
    }

    if (should_swap_turn) {
        if (gs->cuffs_active) {
            game_log(gs, "Handcuffs active! Turn skipped.");
            gs->cuffs_active = 0;
        } else {
            gs->player_turn = !gs->player_turn;
        }
    }

    gs->current_shell++;
    gs->saw_active = 0;

    if (gs->player.charges <= 0 || gs->dealer.charges <= 0) {
        game_log(gs, "MATCH OVER.");
    } else if (gs->current_shell >= gs->n_shells) {
        game_log(gs, "Gun empty. Reloading...");
        generate_shells(gs);
        give_new_items(gs);

        // Force turn to player so the dealer doesn't shoot while showing shells
        gs->player_turn = 1;

        gs->show_shells = 1;
        gs->shell_reveal_time = time(NULL);
    }
}

void use_item(GameState *gs, int item_idx) {
    Player *current_p = gs->player_turn ? &gs->player : &gs->dealer;
    Player *other_p   = gs->player_turn ? &gs->dealer : &gs->player;
    ItemType item = current_p->items[item_idx];

    if (item == ITEM_NONE) return;

    char msg[64];
    snprintf(msg, sizeof(msg), "%s uses %s.", gs->player_turn ? "You" : "Dealer", item_name(item));
    game_log(gs, msg);

    switch (item) {
        case ITEM_BEER:
            if (gs->current_shell < gs->n_shells) {
                ShellType ejected = gs->shells[gs->current_shell];
                game_log(gs, ejected == SHELL_LIVE ? "Ejected a LIVE round." : "Ejected a BLANK.");
                if (ejected == SHELL_LIVE) gs->live_count--;
                else gs->blank_count--;
                gs->current_shell++;
                gs->shells_ejected++; // Track stats
                gs->beer_ml += 330; // Track Beer
            }
            break;

        case ITEM_CIGARETTE:
            if (current_p->charges < current_p->max_charges) {
                current_p->charges++;
                game_log(gs, "Gained 1 charge.");
                gs->cigarettes_smoked++; // Track cigarettes
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

        case ITEM_HANDCUFFS:
            if (!gs->cuffs_active) {
                gs->cuffs_active = 1;
                game_log(gs, "Opponent cuffed! They skip a turn.");
            }
            break;

        case ITEM_MAGNIFYING_GLASS:
            if (gs->current_shell < gs->n_shells) {
                if (gs->player_turn) {
                    ShellType current = gs->shells[gs->current_shell];
                    snprintf(msg, sizeof(msg), "It's a %s.", current == SHELL_LIVE ? "LIVE round" : "BLANK");
                    game_log(gs, msg);
                } else {
                    game_log(gs, "Dealer inspects the chamber.");
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
                game_log(gs, "Bad medicine! -1 charge.");
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
                if (gs->player_turn) {
                    int future_idx = RangeRand(gs->current_shell + 1, gs->n_shells - 1);
                    snprintf(msg, sizeof(msg), "Shell #%d is %s.", future_idx + 1,
                             gs->shells[future_idx] == SHELL_LIVE ? "LIVE" : "BLANK");
                    game_log(gs, msg);
                } else {
                    game_log(gs, "Dealer listens to the phone.");
                }
            } else {
                game_log(gs, gs->player_turn ? "No future shells to check." : "Dealer hears static.");
            }
            break;

        case ITEM_ADRENALINE: {
            if (gs->player_turn) {
                // Check if dealer has anything stealable (excluding adrenaline)
                int has_stealable = 0;
                for (int i = 0; i < MAX_ITEMS; i++) {
                    if (other_p->items[i] != ITEM_NONE &&
                        other_p->items[i] != ITEM_ADRENALINE) {
                        has_stealable = 1;
                        break;
                    }
                }

                if (!has_stealable) {
                    game_log(gs, "Nothing stealable! Adrenaline wasted.");
                } else {
                    gs->adrenaline_active = 1;
                    gs->selected_item = 0;
                    game_log(gs, "ADRENALINE: Select a Dealer item (1-8)");
                }
            } else {
                int stolen_idx = -1;
                for (int i = 0; i < MAX_ITEMS; i++) {
                    if (other_p->items[i] != ITEM_NONE &&
                        other_p->items[i] != ITEM_ADRENALINE) {
                        stolen_idx = i;
                        break;
                    }
                }
                if (stolen_idx != -1) {
                    ItemType stolen = other_p->items[stolen_idx];
                    for (int i = stolen_idx; i < MAX_ITEMS - 1; i++)
                        other_p->items[i] = other_p->items[i + 1];
                    other_p->items[MAX_ITEMS - 1] = ITEM_NONE;

                    for (int j = 0; j < MAX_ITEMS; j++) {
                        if (current_p->items[j] == ITEM_NONE) {
                            current_p->items[j] = stolen;
                            break;
                        }
                    }

                    // Tell the player EXACTLY what was stolen
                    snprintf(msg, sizeof(msg), "Dealer stole your %s!", item_name(stolen));
                    game_log(gs, msg);
                } else {
                    game_log(gs, "Nothing stealable! Adrenaline wasted.");
                }
            }
            break;
        }

        default: break;
    }

    for (int i = item_idx; i < MAX_ITEMS - 1; i++)
        current_p->items[i] = current_p->items[i + 1];
    current_p->items[MAX_ITEMS - 1] = ITEM_NONE;

    if (gs->current_shell >= gs->n_shells) {
        game_log(gs, "Gun empty. Reloading...");
        generate_shells(gs);
        give_new_items(gs);

        gs->player_turn = 1;

        gs->show_shells = 1;
        gs->shell_reveal_time = time(NULL);
    }
}

void dealer_ai_turn(GameState *gs, WINDOW *gwin) {
    int decided_to_shoot = 0;
    int known_shell = SHELL_UNKNOWN;

    game_log(gs, "Dealer is thinking...");

    while (!decided_to_shoot && gs->dealer.charges > 0 && gs->player.charges > 0) {
        int item_to_use = -1;

        // Healing
        for (int i = 0; i < MAX_ITEMS; i++) {
            if (gs->dealer.items[i] == ITEM_CIGARETTE && gs->dealer.charges < gs->dealer.max_charges) {
                item_to_use = i;
                break;
            }
        }

        // Knowledge
        if (item_to_use == -1 && known_shell == SHELL_UNKNOWN) {
            for (int i = 0; i < MAX_ITEMS; i++) {
                if (gs->dealer.items[i] == ITEM_MAGNIFYING_GLASS) {
                    item_to_use = i;
                    known_shell = gs->shells[gs->current_shell];
                    break;
                }
            }
        }

        // Offensive
        if (item_to_use == -1) {
            if (known_shell == SHELL_LIVE) {
                for (int i = 0; i < MAX_ITEMS; i++) {
                    if (gs->dealer.items[i] == ITEM_HANDSAW && !gs->saw_active) {
                        item_to_use = i;
                        break;
                    }
                }
            }
        }

        if (item_to_use != -1) {
            use_item(gs, item_to_use);
            // refresh_dealer_view(gs, gwin);
        } else {
            decided_to_shoot = 1;
        }
    }

    if (gs->dealer.charges > 0 && gs->player.charges > 0) {
        if (known_shell == SHELL_LIVE) fire_shotgun(gs, 0, gwin); // Shoot player
        else if (known_shell == SHELL_BLANK) fire_shotgun(gs, 1, gwin); // Shoot self
        else {
            if (gs->live_count >= gs->blank_count) fire_shotgun(gs, 0, gwin); // Assume Live
            else fire_shotgun(gs, 1, gwin); // Assume Blank
        }
        // refresh_dealer_view(gs, gwin);
    }
}

void init_game(GameState *gs) {
    memset(gs, 0, sizeof(GameState));

    gs->roundAmount = 3;
    int max_charges = RangeRand(2, 4);
    int n_initial_items = RangeRand(2, 4);

    // Initialize Player
    strncpy(gs->player.name, "You", 31);
    gs->player.max_charges = max_charges;
    gs->player.charges = max_charges;

    // Initialize Dealer
    strncpy(gs->dealer.name, "Dealer", 31);
    gs->dealer.max_charges = max_charges;
    gs->dealer.charges = max_charges;

    generate_shells(gs);
    give_new_items(gs);

    // Set starting state
    gs->round = 1;
    gs->player_turn = 1;
    gs->show_shells = 1;
    gs->shell_reveal_time = time(NULL);

    // Log the start
    char msg[64];
    snprintf(msg, sizeof(msg), "Round 1: %d Charges, %d Shells.", max_charges, gs->n_shells);
    game_log(gs, "Welcome to the table.");
    game_log(gs, msg);
}

void start_next_round(GameState *gs) {
    int max_charges = RangeRand(2, 4);
    gs->player.max_charges = max_charges;
    gs->dealer.max_charges = max_charges;
    gs->player.charges     = max_charges;
    gs->dealer.charges     = max_charges;

    for (int i = 0; i < MAX_ITEMS; i++) {
        gs->player.items[i] = ITEM_NONE;
        gs->dealer.items[i] = ITEM_NONE;
    }

    // Reset game state flags
    gs->saw_active        = 0;
    gs->cuffs_active      = 0;
    gs->adrenaline_active = 0;
    gs->player_turn       = 1;
    gs->selected_action   = 0;

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