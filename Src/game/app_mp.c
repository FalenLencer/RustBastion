/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  app_mp.c ─ Sous-système MULTIJOUEUR (extrait de app.c — comportement
 *  identique). Voir app_mp.h pour l'API appelée par app.c.
 */
#include "app_mp.h"
#include "game_init.h"        // game_init_arcade / game_init_custom
#include "../ui/renderer.h"   // render_*, g_map_*, renderer_*_color
#include "../ui/tile_art.h"   // tile_art_draw_*
#include "../ui/ui_utils.h"   // dtxt / mtxt / fh
#include "../engine/audio.h"  // audio_play_menu_music
#include "../map/theme.h"     // theme_get, THEME_COUNT
#include "raylib.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// Multijoueur
#define MP_PORT             47777
#define MP_STATUS_INTERVAL  0.4f    // période d'envoi du statut (s)

// Analyse "IP:port" (port optionnel → MP_PORT). 1 = OK. (mode serveur relais)
int parse_relay_addr(const char *s, uint32_t *ip, uint16_t *port) {
    if (!s || !s[0]) return 0;
    char tmp[32];
    snprintf(tmp, sizeof(tmp), "%s", s);
    int p = MP_PORT;
    char *colon = strchr(tmp, ':');
    if (colon) { *colon = '\0'; p = atoi(colon + 1); }
    if (p <= 0 || p > 65535) p = MP_PORT;
    if (!net_str_to_ip(tmp, ip)) return 0;
    *port = (uint16_t)p;
    return 1;
}

// Duel : monnaie de sabotage (par kill) + ennemis envoyables (type, coût, touche).
#define DUEL_SABOTAGE_PER_KILL  1
// Asym : le défenseur gagne s'il survit N vagues ; l'envahisseur gagne du budget
// avec le temps.
#define ASYM_SURVIVE_SECONDS 180    // Asym PUR : durée à tenir (défenseur gagne)
#define ASYM_BUDGET_PERIOD   0.8f   // +1 de budget toutes les 0.8 s
// Co-op : survie commune ; si l'un des deux boards tombe, tous perdent.
#define COOP_WAVES           20     // vagues à tenir ensemble pour gagner
#define COOP_AID_GOLD        50     // or transféré au partenaire (touche G)
// Ennemi envoyable : type + coût + nom. BOUTONS cliquables (pas de touche →
// fonctionne quel que soit le clavier, AZERTY inclus).
typedef struct { int type; int cost; const char *name; } MpSend;
#define DUEL_SEND_COUNT 4
static const MpSend DUEL_SENDS[DUEL_SEND_COUNT] = {
    { ENEMY_RAIDER,   3, "Raider" },
    { ENEMY_RUNNER,   6, "Runner" },
    { ENEMY_BRUTE,   14, "Brute"  },
    { ENEMY_VEHICLE, 30, "Blinde" },
};
// Asym envahisseur : roster COMPLET (tous les types) + budget par vague.
// Coûts croissants selon la dangerosité ; l'envahisseur compose librement.
#define INV_ROSTER_COUNT 10
static const MpSend INV_ROSTER[INV_ROSTER_COUNT] = {
    { ENEMY_RAIDER,       3, "Raider"  },
    { ENEMY_RUNNER,       6, "Runner"  },
    { ENEMY_MUTANT,      10, "Mutant"  },
    { ENEMY_PATHBREAKER, 12, "Perceur" },
    { ENEMY_BRUTE,       14, "Brute"   },
    { ENEMY_GHOST,       16, "Spectre" },
    { ENEMY_HEALER,      18, "Soigneur"},
    { ENEMY_HUNTER,      20, "Chasseur"},
    { ENEMY_ARTILLERY,   24, "Artilleur"},
    { ENEMY_VEHICLE,     30, "Blinde"  },
};
#define INV_WAVE_BUDGET_BASE 80
#define INV_WAVE_BUDGET_STEP 30
#define INV_WAVE_BUDGET(n)   (INV_WAVE_BUDGET_BASE + (n) * INV_WAVE_BUDGET_STEP)
#define INV_WAVE_COOLDOWN    7.0f   // délai imposé entre deux vagues envoyées (s)

// Lance la partie multijoueur (mode Course) avec le seed partagé.
void mp_launch_game(AppContext *ctx) {
    NetSession *s = &ctx->session;
    ctx->mp_in_game      = 1;
    ctx->mp_peer_valid   = 0;
    ctx->mp_result       = 0;
    ctx->mp_status_timer = 0.0f;
    ctx->mp_sabotage     = 0;
    ctx->mp_prev_kills   = 0;
    ctx->mp_budget_t     = 0.0f;
    ctx->mp_inject_acc    = 0.0f;
    ctx->mp_wave_num      = 0;
    ctx->mp_wave_budget   = INV_WAVE_BUDGET(0);
    ctx->mp_wave_cooldown = 0.0f;
    memset(ctx->mp_wave_compose, 0, sizeof(ctx->mp_wave_compose));
    // ── Config de partie jointe au START (carte/difficulté + rôle Asym) ──
    CustomConfig cfg; memset(&cfg, 0, sizeof(cfg));
    int host_invader = 0;
    if (s->start_extra_len >= (int)sizeof(CustomConfig)) {
        memcpy(&cfg, s->start_extra, sizeof(CustomConfig));
        if (s->start_extra_len > (int)sizeof(CustomConfig))
            host_invader = s->start_extra[sizeof(CustomConfig)] ? 1 : 0;
    }
    // Asym : qui joue l'ENVAHISSEUR (sim figée) selon le choix de l'hôte.
    ctx->mp_invader      = (s->mode == MP_ASYM) && (host_invader ? s->is_host : !s->is_host);
    memset(&ctx->mp_peer, 0, sizeof(ctx->mp_peer));
    // Plateau partagé + panneaux déplaçables : reset.
    ctx->mp_board_tn = 0; ctx->mp_board_un = 0; ctx->mp_board_timer = 0.0f;
    ctx->mp_pos_rival = (Vector2){-1, -1};   // <0 → placement par défaut au 1er rendu
    ctx->mp_pos_duel  = (Vector2){-1, -1};
    ctx->mp_drag      = 0;

    // MÊME seed → même carte (équité). Thème aléatoire résolu de façon
    // déterministe depuis le seed partagé. La config (spawns/minerais/taille/
    // difficulté) choisie par l'hôte s'applique aux DEUX joueurs.
    unsigned int seed = s->seed ? s->seed : 1u;
    if (cfg.theme < 0 || cfg.theme >= THEME_COUNT)
        cfg.theme = (int)(seed % THEME_COUNT);
    SetRandomSeed(seed);
    ctx->active_slot = -1;                 // pas de sauvegarde en multijoueur
    game_init_custom(&ctx->gs, &cfg);

    // Asym PUR : le défenseur ne subit AUCUNE vague d'arcade — uniquement les
    // vagues de l'envahisseur. Victoire = survivre au chrono (ASYM_SURVIVE_SECONDS).
    ctx->mp_asym_timer = 0.0f;
    if (s->mode == MP_ASYM && !ctx->mp_invader) {
        ctx->gs.wave_manager.suppress_auto = 1;   // pas d'auto
        ctx->gs.wave_manager.lock_manual   = 1;   // ni de lancement manuel
    }

    ctx->menu.screen    = MENU_TITLE;
    ctx->screen         = SCREEN_GAME;
    ctx->banner_timer   = 5.0f;
    ctx->gs.ui.show_fps = ctx->menu.opts.show_fps;
    audio_play_theme_music(ctx->gs.map.theme);
}

// Fait apparaître un ennemi injecté par l'adversaire sur un portail. Les
// injections rapprochées sont ÉTALÉES (mp_inject_acc) → effet de vague plutôt
// qu'un bloc instantané.
static void mp_spawn_injected(AppContext *ctx, int etype) {
    GameState *gs = &ctx->gs;
    if (etype < 0 || etype >= ENEMY_TYPE_COUNT) return;
    int path = -1;
    for (int p = 0; p < gs->enemy_paths.count; p++)
        if (gs->enemy_paths.paths[p].found) { path = p; break; }
    if (path < 0) return;
    const Theme *th = theme_get(gs->map.theme);
    float delay = ctx->mp_inject_acc;
    ctx->mp_inject_acc += 0.45f;
    if (ctx->mp_inject_acc > 12.0f) ctx->mp_inject_acc = 12.0f;
    enemy_spawn(&gs->enemies, (EnemyType)etype, path, &gs->enemy_paths,
                delay, gs->wave_manager.scale,
                th->enemy_speed_mult * gs->wave_manager.speed_mult);
}

// Rectangle du bouton d'envoi Duel n°i (bandeau haut-centre).
// Dimensions du panneau Duel (grip de glissement + rangée de boutons).
#define MP_DUEL_BARW  (DUEL_SEND_COUNT*100 + (DUEL_SEND_COUNT-1)*8)  // 424
#define MP_DUEL_GRIP_H 18
#define MP_RIVAL_W    240
#define MP_RIVAL_H     34

static Rectangle mp_duel_btn_rect(const AppContext *ctx, int i) {
    int bw = 100, bh = 30, gap = 8;
    float bx = ctx->mp_pos_duel.x;
    float by = ctx->mp_pos_duel.y + MP_DUEL_GRIP_H + 2;   // sous le grip
    return (Rectangle){ bx + (float)(i * (bw + gap)), by, (float)bw, (float)bh };
}

// Duel : clic sur un bouton d'envoi (traité AVANT l'input HUD → bloque le
// placement de tour sous le bouton via ui.mp_block_click).
void mp_duel_send_input(AppContext *ctx) {
    if (ctx->mp_mode != MP_DUEL || !ctx->mp_in_game ||
        ctx->mp_result != 0 || ctx->menu.paused || ctx->mp_drag) return;
    if (!IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) return;
    Vector2 vm = virt_mouse();
    for (int i = 0; i < DUEL_SEND_COUNT; i++) {
        if (CheckCollisionPointRec(vm, mp_duel_btn_rect(ctx, i))) {
            if (ctx->mp_sabotage >= DUEL_SENDS[i].cost) {
                ctx->mp_sabotage -= DUEL_SENDS[i].cost;
                uint8_t t = (uint8_t)DUEL_SENDS[i].type;
                net_session_send(&ctx->session, NMSG_SEND_ENEMY, &t, 1);
                ui_push_notif(&ctx->gs.ui, TextFormat("%s envoye !", DUEL_SENDS[i].name),
                              (Color){200, 90, 200, 255});
            } else {
                ui_push_notif(&ctx->gs.ui, "Sabotage insuffisant", (Color){231, 76, 60, 255});
            }
            ctx->gs.ui.mp_block_click = 1;   // ne pas placer de tour sous le bouton
            return;
        }
    }
}

// Glissement des panneaux flottants MP (HUD rival + barre Duel). Traité AVANT
// l'input HUD pour ne pas placer de tour en déplaçant un panneau.
void mp_panels_input(AppContext *ctx) {
    if (!ctx->mp_in_game || ctx->mp_invader || ctx->mp_result != 0 || ctx->menu.paused) return;
    int base_cx = g_map_x_off + g_canvas_virt_w_base / 2;
    if (ctx->mp_pos_rival.x < 0) ctx->mp_pos_rival = (Vector2){ (float)(base_cx - MP_RIVAL_W/2), 4.0f };
    if (ctx->mp_pos_duel.x  < 0) ctx->mp_pos_duel  = (Vector2){ (float)(base_cx - MP_DUEL_BARW/2), 44.0f };

    Vector2 m = virt_mouse();
    Rectangle rival = { ctx->mp_pos_rival.x, ctx->mp_pos_rival.y, MP_RIVAL_W, MP_RIVAL_H };
    Rectangle dgrip = { ctx->mp_pos_duel.x,  ctx->mp_pos_duel.y,  MP_DUEL_BARW, MP_DUEL_GRIP_H };

    if (ctx->mp_drag == 0 && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        if (CheckCollisionPointRec(m, rival)) {
            ctx->mp_drag = 1;
            ctx->mp_drag_grab = (Vector2){ m.x - ctx->mp_pos_rival.x, m.y - ctx->mp_pos_rival.y };
        } else if (ctx->mp_mode == MP_DUEL && CheckCollisionPointRec(m, dgrip)) {
            ctx->mp_drag = 2;
            ctx->mp_drag_grab = (Vector2){ m.x - ctx->mp_pos_duel.x, m.y - ctx->mp_pos_duel.y };
        }
    }
    if (ctx->mp_drag != 0) {
        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
            Vector2 *p = (ctx->mp_drag == 1) ? &ctx->mp_pos_rival : &ctx->mp_pos_duel;
            p->x = m.x - ctx->mp_drag_grab.x;
            p->y = m.y - ctx->mp_drag_grab.y;
            if (p->x < 0) p->x = 0;
            if (p->y < 0) p->y = 0;
            if (p->x > g_canvas_virt_w - 40)  p->x = (float)(g_canvas_virt_w - 40);
            if (p->y > g_canvas_virt_h - 24)  p->y = (float)(g_canvas_virt_h - 24);
            ctx->gs.ui.mp_block_click = 1;   // ne pas placer de tour pendant le drag
        } else {
            ctx->mp_drag = 0;
        }
    }
}

// Fixe le résultat local (won=1 → VICTOIRE) et l'annonce au pair (octet = vainqueur).
static void mp_set_result(AppContext *ctx, int won) {
    if (ctx->mp_result != 0) return;
    ctx->mp_result = won ? 1 : 2;
    uint8_t w = won ? 1 : 0;
    net_session_send(&ctx->session, NMSG_GAMEOVER, &w, 1);
}

// Tick réseau en jeu (Course / Duel / Asym). Tourne même en pause (keep-alive).
void mp_game_tick(AppContext *ctx, float dt) {
    if (!ctx->mp_active || !ctx->mp_in_game) return;
    NetSession *s = &ctx->session;
    int inv = ctx->mp_invader;
    net_session_update(s);

    NetMsgHeader h;
    unsigned char pl[NET_MAX_PAYLOAD];
    while (net_session_recv(s, &h, pl, sizeof(pl))) {
        if (h.type == NMSG_STATUS && h.len >= (int)sizeof(NetStatus)) {
            memcpy(&ctx->mp_peer, pl, sizeof(NetStatus));
            ctx->mp_peer_valid = 1;
        } else if (h.type == NMSG_GAMEOVER) {
            int peer_won = (h.len >= 1) ? pl[0] : 0;
            if (ctx->mp_result == 0)
                ctx->mp_result = (ctx->mp_mode == MP_COOP)
                               ? (peer_won ? 1 : 2)    // co-op : sort partagé (adopté)
                               : (peer_won ? 2 : 1);   // versus : inversé
        } else if (h.type == NMSG_SEND_ENEMY && h.len >= 1 && !inv) {
            mp_spawn_injected(ctx, pl[0]);   // on subit l'envoi (défenseur / Duel)
        } else if (h.type == NMSG_AID && h.len >= 4) {
            int32_t g; memcpy(&g, pl, 4);
            if (g > 0 && g < 100000) {       // co-op : or reçu du partenaire
                ctx->gs.gold += g;
                ui_push_notif(&ctx->gs.ui, "Or recu du partenaire !", (Color){120,210,120,255});
            }
        } else if (h.type == NMSG_BOARD && inv) {
            // Asym : snapshot du plateau du défenseur (tours + unités).
            int o = 0, len = h.len;
            ctx->mp_board_tn = 0; ctx->mp_board_un = 0;
            if (o < len) {
                int nt = pl[o++]; if (nt > 64) nt = 64;
                if (o + nt * 3 <= len) {
                    memcpy(ctx->mp_board_t, pl + o, nt * 3);
                    ctx->mp_board_tn = nt; o += nt * 3;
                }
            }
            if (o < len) {
                int nu = pl[o++]; if (nu > 32) nu = 32;
                if (o + nu * 3 <= len) {
                    memcpy(ctx->mp_board_u, pl + o, nu * 3);
                    ctx->mp_board_un = nu;
                }
            }
        }
    }
    if (s->peer_gone && ctx->mp_result == 0)
        ctx->mp_result = (ctx->mp_mode == MP_COOP) ? 2 : 1;  // co-op : sans partenaire = perdu

    // Étalement des ennemis injectés : décroît quand on n'en reçoit plus.
    if (ctx->mp_inject_acc > 0.0f) {
        ctx->mp_inject_acc -= dt;
        if (ctx->mp_inject_acc < 0.0f) ctx->mp_inject_acc = 0.0f;
    }
    // Asym envahisseur : cooldown entre vagues.
    if (inv && ctx->mp_wave_cooldown > 0.0f) {
        ctx->mp_wave_cooldown -= dt;
        if (ctx->mp_wave_cooldown < 0.0f) ctx->mp_wave_cooldown = 0.0f;
    }

    // Duel : sabotage gagné aux kills (les ENVOIS se font aux BOUTONS, cf.
    // mp_duel_send_input). L'envahisseur Asym compose ses vagues (mp_invader_render).
    if (ctx->mp_mode == MP_DUEL && ctx->mp_result == 0 && !ctx->menu.paused) {
        int dk = ctx->gs.kills - ctx->mp_prev_kills;
        if (dk > 0) {
            ctx->mp_sabotage += dk * DUEL_SABOTAGE_PER_KILL;
            ctx->mp_prev_kills = ctx->gs.kills;
        }
    }

    // ── Co-op : entraide (transfert d'or au partenaire, touche G) ──
    if (ctx->mp_mode == MP_COOP && !inv && ctx->mp_result == 0 && !ctx->menu.paused) {
        if (IsKeyPressed(KEY_G) && ctx->gs.gold >= COOP_AID_GOLD) {
            ctx->gs.gold -= COOP_AID_GOLD;
            int32_t g = COOP_AID_GOLD;
            net_session_send(s, NMSG_AID, &g, 4);
            ui_push_notif(&ctx->gs.ui, "+50 or envoye au partenaire", (Color){120,210,120,255});
        }
    }

    // ── Asym : le DÉFENSEUR streame son plateau à l'envahisseur (plateau partagé) ──
    if (ctx->mp_mode == MP_ASYM && !inv && !ctx->menu.paused) {
        ctx->mp_board_timer -= dt;
        if (ctx->mp_board_timer <= 0.0f) {
            ctx->mp_board_timer = 0.3f;
            unsigned char b[NET_MAX_PAYLOAD];
            int o = 0, nt = 0, nu = 0;
            int nt_pos = o++;                       // emplacement du compteur tours
            TowerPool *tp = &ctx->gs.towers;
            for (int i = 0; i < tp->tower_count && nt < 64; i++) {
                Tower *t = &tp->towers[i];
                if (!t->active) continue;
                b[o++] = (unsigned char)t->tile_x;
                b[o++] = (unsigned char)t->tile_y;
                b[o++] = (unsigned char)t->type;
                nt++;
            }
            b[nt_pos] = (unsigned char)nt;
            int nu_pos = o++;                       // emplacement du compteur unités
            UnitPool *up = &ctx->gs.units;
            for (int i = 0; i < up->count && nu < 32; i++) {
                Unit *u = &up->units[i];
                if (!u->active) continue;
                b[o++] = (unsigned char)((int)(u->x / TILE_SIZE));
                b[o++] = (unsigned char)((int)(u->y / TILE_SIZE));
                b[o++] = (unsigned char)u->type;
                nu++;
            }
            b[nu_pos] = (unsigned char)nu;
            net_session_send(s, NMSG_BOARD, b, o);
        }
    }

    // Asym PUR (défenseur) : pas de vagues d'arcade, mais board ACTIF (les
    // ouvriers minent en PHASE_WAVE) + chrono de survie (il gagne s'il tient).
    if (ctx->mp_mode == MP_ASYM && !inv) {
        ctx->gs.phase = PHASE_WAVE;
        if (ctx->mp_result == 0 && !ctx->menu.paused)
            ctx->mp_asym_timer += dt;
    }

    // ── Statut : seuls les joueurs AVEC un board l'envoient (pas l'envahisseur) ──
    if (!inv && !ctx->menu.paused) {
        ctx->mp_status_timer -= dt;
        if (ctx->mp_status_timer <= 0.0f) {
            ctx->mp_status_timer = MP_STATUS_INTERVAL;
            NetStatus st;
            st.wave  = ctx->gs.wave_manager.number;
            if (ctx->mp_mode == MP_ASYM) {       // Asym : `wave` = secondes restantes
                int rem = (int)((float)ASYM_SURVIVE_SECONDS - ctx->mp_asym_timer + 0.5f);
                st.wave = rem < 0 ? 0 : rem;
            }
            st.kills = ctx->gs.kills;
            st.gold  = ctx->gs.gold;
            st.lives = ctx->gs.lives;
            st.alive = (ctx->gs.lives > 0) ? 1 : 0;
            net_session_send(s, NMSG_STATUS, &st, sizeof(st));
        }
    }

    // ── Conditions de fin ──
    if (ctx->mp_result == 0 && !inv) {
        if (ctx->mp_mode == MP_COOP) {
            // L'HÔTE arbitre le sort partagé (les deux gagnent ou perdent ensemble).
            if (ctx->session.is_host) {
                int peer_dead = (ctx->mp_peer_valid && ctx->mp_peer.lives <= 0);
                if (ctx->gs.lives <= 0 || peer_dead)
                    mp_set_result(ctx, 0);                          // un board tombe → tous perdent
                else if (ctx->gs.wave_manager.number >= COOP_WAVES &&
                         ctx->mp_peer_valid && ctx->mp_peer.wave >= COOP_WAVES)
                    mp_set_result(ctx, 1);                          // survie commune → tous gagnent
            }
            // le client attend le verdict de l'hôte (GAMEOVER)
        } else if (ctx->gs.lives <= 0) {
            mp_set_result(ctx, 0);                                  // mes bases tombent → je perds
        } else if (ctx->mp_mode == MP_ASYM &&
                   ctx->mp_asym_timer >= (float)ASYM_SURVIVE_SECONDS) {
            mp_set_result(ctx, 1);                                  // défenseur a tenu → gagne
        }
    }
}

// Rectangles des boutons du bandeau de résultat (partagés rendu + input).
static Rectangle mp_result_btn(int which) {  // 0 = REMATCH, 1 = MENU
    int cx = g_canvas_virt_w / 2, cy = g_canvas_virt_h / 2;
    return (which == 0) ? (Rectangle){(float)(cx - 220), (float)(cy + 12), 200.0f, 46.0f}
                        : (Rectangle){(float)(cx + 20),  (float)(cy + 12), 200.0f, 46.0f};
}

// Bandeau de résultat VICTOIRE / DEFAITE + boutons REMATCH / MENU.
static void mp_draw_result(AppContext *ctx) {
    if (ctx->mp_result == 0) return;
    int cx = g_canvas_virt_w / 2, cy = g_canvas_virt_h / 2;
    DrawRectangle(0, 0, g_canvas_virt_w, g_canvas_virt_h, (Color){0, 0, 0, 188});
    const char *t = (ctx->mp_result == 1) ? "VICTOIRE" : "DEFAITE";
    Color tc = (ctx->mp_result == 1) ? (Color){240, 200, 60, 255}
                                     : (Color){220, 70, 60, 255};
    int tw = mtxt(t, 40);
    dtxt(t, cx - tw/2, cy - 72, 40, tc);

    int can_rematch = net_session_connected(&ctx->session);
    Vector2 vm = virt_mouse();
    Rectangle rb = mp_result_btn(0), mb = mp_result_btn(1);
    int hov_r = can_rematch && CheckCollisionPointRec(vm, rb);
    int hov_m = CheckCollisionPointRec(vm, mb);

    DrawRectangleRounded(rb, 0.3f, 5, !can_rematch ? (Color){25,22,18,225}
                                    : hov_r ? (Color){40,120,60,245} : (Color){22,70,38,238});
    DrawRectangleRoundedLinesEx(rb, 0.3f, 5, hov_r ? 2.2f : 1.2f,
                                can_rematch ? (Color){80,200,110,235} : (Color){70,65,55,180});
    int rw = mtxt("REMATCH", 14);
    dtxt("REMATCH", (int)rb.x + 100 - rw/2, (int)rb.y + 23 - fh(14)/2, 14,
         can_rematch ? (Color){200,240,200,255} : (Color){100,95,85,255});

    DrawRectangleRounded(mb, 0.3f, 5, hov_m ? (Color){72,56,20,245} : (Color){46,36,12,238});
    DrawRectangleRoundedLinesEx(mb, 0.3f, 5, hov_m ? 2.2f : 1.2f, (Color){150,120,40,225});
    int mw = mtxt("MENU", 14);
    dtxt("MENU", (int)mb.x + 100 - mw/2, (int)mb.y + 23 - fh(14)/2, 14, (Color){230,200,140,255});

    const char *hint = can_rematch ? "REMATCH : rejouer ensemble  -  MENU : quitter"
                                   : "Adversaire deconnecte  -  MENU pour revenir";
    int hw = mtxt(hint, 9);
    dtxt(hint, cx - hw/2, (int)rb.y + 56, 9, (Color){150, 130, 90, 230});
}

// Clics sur le bandeau de résultat (REMATCH garde la session, MENU la ferme).
void mp_handle_result(AppContext *ctx) {
    if (ctx->mp_result == 0) return;
    Vector2 vm = virt_mouse();
    int click = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
    int can_rematch = net_session_connected(&ctx->session);

    if (click && can_rematch && CheckCollisionPointRec(vm, mp_result_btn(0))) {
        net_session_rematch(&ctx->session);      // retour au salon, session conservée
        ctx->mp_in_game = 0;
        ctx->mp_invader = 0;
        ctx->mp_result  = 0;
        ctx->menu.paused = 0;
        ctx->screen      = SCREEN_MENU;
        ctx->menu.screen = MENU_MP_LOBBY;
        audio_play_menu_music();
    } else if ((click && CheckCollisionPointRec(vm, mp_result_btn(1))) ||
               IsKeyPressed(KEY_ESCAPE)) {
        net_session_close(&ctx->session);
        ctx->mp_active   = 0;
        ctx->mp_in_game  = 0;
        ctx->mp_invader  = 0;
        ctx->menu.paused = 0;
        ctx->screen      = SCREEN_MENU;
        ctx->menu.screen = MENU_MP_HUB;
        audio_play_menu_music();
    }
}

// ════════════════════════════════════════════════════════════════
// ÉCRAN JEU — RENDU
// retourne 0 si l'utilisateur demande à quitter (pause → quitter)
// ════════════════════════════════════════════════════════════════
// Overlay multijoueur : mini-HUD du rival + bandeau de résultat.
void mp_render_overlay(AppContext *ctx) {
    if (!ctx->mp_in_game) return;
    int base_cx = g_map_x_off + g_canvas_virt_w_base / 2;
    if (ctx->mp_pos_rival.x < 0) ctx->mp_pos_rival = (Vector2){ (float)(base_cx - MP_RIVAL_W/2), 4.0f };
    if (ctx->mp_pos_duel.x  < 0) ctx->mp_pos_duel  = (Vector2){ (float)(base_cx - MP_DUEL_BARW/2), 44.0f };

    const char *nm = ctx->session.peer_name[0] ? ctx->session.peer_name : "Adversaire";
    char top[80], cmp[96];
    if (ctx->mp_mode == MP_ASYM) {           // défenseur : survie au chrono
        int rem = (int)((float)ASYM_SURVIVE_SECONDS - ctx->mp_asym_timer + 0.5f);
        if (rem < 0) rem = 0;
        snprintf(top, sizeof(top), "ENVAHISSEUR : %s", nm);
        snprintf(cmp, sizeof(cmp), "Survie : tenir encore %d:%02d     PV %d",
                 rem / 60, rem % 60, ctx->gs.lives);
    } else if (ctx->mp_mode == MP_COOP) {    // partenaire : sort partagé + entraide
        snprintf(top, sizeof(top), "PARTENAIRE : %s   [G]=+50or", nm);
        if (ctx->mp_peer_valid)
            snprintf(cmp, sizeof(cmp), "Vous V%d PV%d  +  Lui V%d PV%d   (objectif V%d)",
                     ctx->gs.wave_manager.number, ctx->gs.lives,
                     ctx->mp_peer.wave, ctx->mp_peer.lives, COOP_WAVES);
        else
            snprintf(cmp, sizeof(cmp), "Vous V%d PV%d   |   Lui (connexion...)",
                     ctx->gs.wave_manager.number, ctx->gs.lives);
    } else {
        snprintf(top, sizeof(top), "RIVAL : %s", nm);
        if (ctx->mp_peer_valid)
            snprintf(cmp, sizeof(cmp), "Vous V%d PV%d   |   Lui V%d PV%d",
                     ctx->gs.wave_manager.number, ctx->gs.lives,
                     ctx->mp_peer.wave, ctx->mp_peer.lives);
        else
            snprintf(cmp, sizeof(cmp), "Vous V%d PV%d   |   Lui (connexion...)",
                     ctx->gs.wave_manager.number, ctx->gs.lives);
    }
    // Panneau rival déplaçable — largeur adaptée au texte (aucun débordement).
    int tw0 = mtxt(top, 9), cw0 = mtxt(cmp, 8);
    int twmax = tw0 > cw0 ? tw0 : cw0;
    int pw = twmax + 24; if (pw < MP_RIVAL_W) pw = MP_RIVAL_W;
    int ph = MP_RIVAL_H;
    int rx = (int)ctx->mp_pos_rival.x, ry = (int)ctx->mp_pos_rival.y;
    int rcx = rx + pw/2;
    Rectangle rpanel = {(float)rx,(float)ry,(float)pw,(float)ph};
    DrawRectangleRounded(rpanel, 0.22f, 5, (Color){10, 7, 3, 222});
    DrawRectangleRoundedLinesEx(rpanel, 0.22f, 5, 1.2f, (Color){70, 48, 16, 210});
    // 2 lignes empilées via fh() — pas de superposition.
    dtxt(top, rcx - tw0/2, ry + 3, 9, (Color){200, 180, 120, 255});
    dtxt(cmp, rcx - cw0/2, ry + 3 + fh(9), 8, (Color){150, 130, 80, 255});

    // Panneau Duel déplaçable : grip (sabotage) + BOUTONS d'envoi cliquables
    if (ctx->mp_mode == MP_DUEL && ctx->mp_result == 0) {
        int dx = (int)ctx->mp_pos_duel.x, dy = (int)ctx->mp_pos_duel.y;
        Rectangle grip = {(float)dx,(float)dy,(float)MP_DUEL_BARW,(float)MP_DUEL_GRIP_H};
        DrawRectangleRounded(grip, 0.4f, 4, (Color){26, 12, 32, 225});
        char sb[64];
        snprintf(sb, sizeof(sb), ":::  SABOTAGE %d  -  envoyez des ennemis", ctx->mp_sabotage);
        int sw = mtxt(sb, 8);
        dtxt(sb, dx + MP_DUEL_BARW/2 - sw/2, dy + (MP_DUEL_GRIP_H - fh(8))/2, 8,
             (Color){220, 140, 230, 255});
        Vector2 vm = virt_mouse();
        for (int i = 0; i < DUEL_SEND_COUNT; i++) {
            Rectangle r = mp_duel_btn_rect(ctx, i);
            int afford = (ctx->mp_sabotage >= DUEL_SENDS[i].cost);
            int hov    = afford && CheckCollisionPointRec(vm, r);
            Color bg  = !afford ? (Color){20, 16, 22, 225}
                      : hov     ? (Color){64, 30, 74, 240}
                                : (Color){30, 14, 38, 230};
            Color brd = afford ? (Color){175, 85, 195, 235} : (Color){70, 60, 75, 200};
            DrawRectangleRounded(r, 0.25f, 4, bg);
            DrawRectangleRoundedLinesEx(r, 0.25f, 4, hov ? 2.0f : 1.2f, brd);
            char b[28];
            snprintf(b, sizeof(b), "%s  -%d", DUEL_SENDS[i].name, DUEL_SENDS[i].cost);
            int bw2 = mtxt(b, 9);
            dtxt(b, (int)r.x + (int)r.width/2 - bw2/2, (int)r.y + (int)r.height/2 - fh(9)/2, 9,
                 afford ? (Color){210, 235, 180, 255} : (Color){110, 100, 110, 255});
        }
    }

    mp_draw_result(ctx);   // bandeau VICTOIRE / DEFAITE
}

// Écran de l'ENVAHISSEUR (Asym) : carte figée + panneau de commande + statut défenseur.
void mp_invader_render(AppContext *ctx) {
    GameState *gs = &ctx->gs;
    int cx = g_canvas_virt_w / 2;
    int base_cx = g_map_x_off + g_canvas_virt_w_base / 2;
    Vector2 vm = virt_mouse();
    int click = (ctx->mp_result == 0) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);

    // ── Carte figée (référence visuelle des portails/bases/chemins) ──
    ClearBackground(theme_get(gs->map.theme)->palette.bg);
    Camera2D cam = {0};
    cam.offset = (Vector2){(float)g_map_x_off, 0.0f};
    cam.zoom   = g_map_render_scale;
    BeginMode2D(cam);
        render_map(&gs->map);
        tile_art_draw_paths(&gs->map);
        tile_art_draw_spawns(&gs->map);
        render_spawn_exclusion_zones(&gs->map);
        render_bases(&gs->map);
        // ── PLATEAU PARTAGÉ : tours + unités du défenseur (marqueurs live) ──
        for (int i = 0; i < ctx->mp_board_tn; i++) {
            int tx = ctx->mp_board_t[i*3], ty = ctx->mp_board_t[i*3+1];
            int tt = ctx->mp_board_t[i*3+2];
            Color c = renderer_tower_color((TowerType)tt);
            Rectangle rr = {tx*(float)TILE_SIZE + 5, ty*(float)TILE_SIZE + 5,
                            TILE_SIZE - 10.0f, TILE_SIZE - 10.0f};
            DrawRectangleRounded(rr, 0.3f, 4, c);
            DrawRectangleRoundedLinesEx(rr, 0.3f, 4, 2.0f, (Color){10, 8, 6, 230});
        }
        for (int i = 0; i < ctx->mp_board_un; i++) {
            int tx = ctx->mp_board_u[i*3], ty = ctx->mp_board_u[i*3+1];
            int ut = ctx->mp_board_u[i*3+2];
            Color c = renderer_unit_color((UnitType)ut);
            float px = tx*(float)TILE_SIZE + TILE_SIZE/2.0f;
            float py = ty*(float)TILE_SIZE + TILE_SIZE/2.0f;
            DrawCircle((int)px, (int)py, 6.0f, c);
            DrawCircleLines((int)px, (int)py, 6.0f, (Color){10, 8, 6, 230});
        }
    EndMode2D();

    // ── En-tête + statut du défenseur + légende plateau ──
    const char *title = "ENVAHISSEUR — composez vos vagues";
    int tw = mtxt(title, 13);
    dtxt(title, cx - tw/2, 6, 13, (Color){220, 140, 230, 255});
    char ds[96];
    if (ctx->mp_peer_valid) {
        int rem = ctx->mp_peer.wave;   // Asym : `wave` = secondes restantes (cf. STATUS)
        if (rem < 0) rem = 0;
        snprintf(ds, sizeof(ds), "Defenseur %s : tient encore %d:%02d     PV %d  -  perce-le !",
                 ctx->session.peer_name[0] ? ctx->session.peer_name : "?",
                 rem / 60, rem % 60, ctx->mp_peer.lives);
    } else {
        snprintf(ds, sizeof(ds), "Connexion au defenseur...");
    }
    int dw = mtxt(ds, 11);
    dtxt(ds, cx - dw/2, 6 + fh(13) + 4, 11, (Color){200, 180, 120, 255});
    const char *leg = "Carte LIVE  -  carres = tours, points = unites du defenseur";
    int lw = mtxt(leg, 8);
    dtxt(leg, cx - lw/2, 6 + fh(13) + 4 + fh(11) + 3, 8, (Color){150, 130, 160, 255});

    // ── Compositeur de vague (bas de l'écran) : 2 rangées de 5 ──
    int bw = 120, bh = 28, gap = 6;
    int cols = 5;
    int rowsw = cols*bw + (cols-1)*gap;             // largeur d'une rangée
    int panel_h = 162, panel_y = g_canvas_virt_h - panel_h - 8;
    int pw = rowsw + 56, px = base_cx - pw/2;
    DrawRectangleRounded((Rectangle){(float)px,(float)panel_y,(float)pw,(float)panel_h},
                         0.06f, 6, (Color){12, 6, 16, 236});
    DrawRectangleRoundedLinesEx((Rectangle){(float)px,(float)panel_y,(float)pw,(float)panel_h},
                         0.06f, 6, 1.5f, (Color){120, 50, 140, 220});

    int max_budget = INV_WAVE_BUDGET(ctx->mp_wave_num);
    char hdr[64];
    snprintf(hdr, sizeof(hdr), "VAGUE %d    Budget : %d / %d",
             ctx->mp_wave_num + 1, ctx->mp_wave_budget, max_budget);
    int hw = mtxt(hdr, 11);
    dtxt(hdr, base_cx - hw/2, panel_y + 6, 11, (Color){220, 160, 240, 255});

    // Roster cliquable (ajoute un ennemi à la vague) — 2 rangées de 5
    int rx0 = base_cx - rowsw/2, ry0 = panel_y + 6 + fh(11) + 4;
    for (int i = 0; i < INV_ROSTER_COUNT; i++) {
        int col = i % cols, row = i / cols;
        Rectangle r = {(float)(rx0 + col*(bw+gap)), (float)(ry0 + row*(bh+gap)),
                       (float)bw, (float)bh};
        int afford = ctx->mp_wave_budget >= INV_ROSTER[i].cost;
        int hov    = afford && CheckCollisionPointRec(vm, r);
        if (click && hov) {
            ctx->mp_wave_compose[INV_ROSTER[i].type]++;
            ctx->mp_wave_budget -= INV_ROSTER[i].cost;
        }
        Color bg  = !afford ? (Color){18,14,20,230} : hov ? (Color){62,30,72,240} : (Color){28,14,36,235};
        Color brd = afford ? (Color){175,85,195,235} : (Color){70,60,75,200};
        DrawRectangleRounded(r, 0.25f, 4, bg);
        DrawRectangleRoundedLinesEx(r, 0.25f, 4, hov ? 2.0f : 1.2f, brd);
        char b[28]; snprintf(b, sizeof(b), "%s -%d", INV_ROSTER[i].name, INV_ROSTER[i].cost);
        int b2 = mtxt(b, 9);
        dtxt(b, (int)r.x + bw/2 - b2/2, (int)r.y + bh/2 - fh(9)/2, 9,
             afford ? (Color){210,235,180,255} : (Color){110,100,110,255});
    }
    int roster_bottom = ry0 + 2*bh + gap;           // bas de la 2e rangée

    // Résumé de la vague composée
    char comp[200]; snprintf(comp, sizeof(comp), "%s", "Votre vague : ");
    int any = 0, tcount = 0;
    for (int i = 0; i < INV_ROSTER_COUNT; i++) {
        int n = ctx->mp_wave_compose[INV_ROSTER[i].type];
        if (n > 0) {
            char piece[40]; snprintf(piece, sizeof(piece), "%s%dx %s", any ? ", " : "", n, INV_ROSTER[i].name);
            strncat(comp, piece, sizeof(comp) - strlen(comp) - 1);
            any = 1; tcount += n;
        }
    }
    if (!any) strncat(comp, "(vide - cliquez des ennemis ci-dessus)", sizeof(comp) - strlen(comp) - 1);
    int cw = mtxt(comp, 9);
    dtxt(comp, base_cx - cw/2, roster_bottom + 6, 9, (Color){180,170,190,255});

    // Boutons ENVOYER / VIDER
    int by = roster_bottom + 6 + fh(9) + 6;
    Rectangle bsend = {(float)(base_cx - 170), (float)by, 200.0f, 30.0f};
    Rectangle bclr  = {(float)(base_cx + 40),  (float)by, 130.0f, 30.0f};
    int ready    = (ctx->mp_wave_cooldown <= 0.0f);
    int can_send = (tcount > 0) && ready;
    int hov_s = can_send && CheckCollisionPointRec(vm, bsend);
    int hov_c = (tcount > 0) && CheckCollisionPointRec(vm, bclr);

    DrawRectangleRounded(bsend, 0.3f, 5, hov_s ? (Color){40,120,60,240}
                                       : can_send ? (Color){20,70,35,235} : (Color){25,22,18,220});
    DrawRectangleRoundedLinesEx(bsend, 0.3f, 5, hov_s ? 2.0f : 1.2f,
                                can_send ? (Color){80,200,110,235} : (Color){70,65,55,200});
    char eb[48];
    if (!ready)  snprintf(eb, sizeof(eb), "RECHARGE  %.0fs", ctx->mp_wave_cooldown + 0.9f);
    else         snprintf(eb, sizeof(eb), "ENVOYER LA VAGUE (%d)", tcount);
    int ew = mtxt(eb, 10);
    dtxt(eb, (int)bsend.x + 100 - ew/2, (int)bsend.y + 15 - fh(10)/2, 10,
         can_send ? (Color){200,240,200,255} : (Color){110,105,95,255});

    DrawRectangleRounded(bclr, 0.3f, 5, hov_c ? (Color){90,40,40,235} : (Color){40,22,22,225});
    DrawRectangleRoundedLinesEx(bclr, 0.3f, 5, hov_c ? 2.0f : 1.2f, (Color){160,70,70,210});
    int vw2 = mtxt("VIDER", 10);
    dtxt("VIDER", (int)bclr.x + 65 - vw2/2, (int)bclr.y + 15 - fh(10)/2, 10, (Color){220,160,160,255});

    if (click && hov_s) {              // envoie la vague composée
        for (int i = 0; i < INV_ROSTER_COUNT; i++) {
            int t = INV_ROSTER[i].type;
            for (int k = 0; k < ctx->mp_wave_compose[t]; k++) {
                uint8_t et = (uint8_t)t;
                net_session_send(&ctx->session, NMSG_SEND_ENEMY, &et, 1);
            }
        }
        memset(ctx->mp_wave_compose, 0, sizeof(ctx->mp_wave_compose));
        ctx->mp_wave_num++;
        ctx->mp_wave_budget = INV_WAVE_BUDGET(ctx->mp_wave_num);
        ctx->mp_wave_cooldown = INV_WAVE_COOLDOWN;   // pacing entre vagues
    } else if (click && hov_c) {       // vide la composition (rembourse)
        memset(ctx->mp_wave_compose, 0, sizeof(ctx->mp_wave_compose));
        ctx->mp_wave_budget = INV_WAVE_BUDGET(ctx->mp_wave_num);
    }

    mp_draw_result(ctx);
}
