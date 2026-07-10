/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  hero.c ─ MODE HÉROS : avatar (déplacement, caméra, pause, HUD) et
 *  orchestration de la frame. Les interactions (tir, achats, placement)
 *  sont dans hero_actions.c ; la scène 3D dans ui/render3d_world.c.
 */
#include "hero.h"
#include "app.h"
#include "../ui/render3d_world.h"
#include "../ui/renderer.h"      /* g_canvas_virt_w/h                     */
#include "../ui/ui_utils.h"      /* dtxt / mtxt / fh                      */
#include "../ui/hud_internal.h"  /* virt_mouse (transform souris fiable)  */
#include "../combat/fx.h"
#include "../engine/audio.h"
#include "../engine/window.h"    /* window_apply_size (menu pause)        */
#include <math.h>
#include <stdio.h>

#define HERO_TOAST_TIME    2.4f   /* durée d'affichage d'un message (s)   */

static int hero_tile_blocked(const GameState *gs, int tx, int ty, float feet_h);

/* ════════════════════════════════════════════════════════════════
   DÉMARRAGE / SORTIE
   ════════════════════════════════════════════════════════════════ */
void hero_toast(HeroState *h, const char *msg, Color col) {
    snprintf(h->toast, sizeof(h->toast), "%s", msg);
    h->toast_col = col;
    h->toast_t   = HERO_TOAST_TIME;
}

/* Capture de la souris. Deux modes (Options > Commandes) :
   - NATIF : DisableCursor + GetMouseDelta = feel FPS standard (défaut).
   - COMPATIBLE : curseur caché, recentré chaque frame, regard = écart au
     centre via virt_mouse (repli pour les configs où le natif s'emballe). */
static void hero_mouse_capture(HeroState *h, int native) {
    h->mouse_native = native ? 1 : 0;
    if (native) {
        DisableCursor();
    } else {
        HideCursor();
        SetMousePosition(GetScreenWidth() / 2, GetScreenHeight() / 2);
    }
    h->mouse_free   = 0;
    h->mouse_settle = 3;    /* ignore les 1ers deltas (transition capture) */
}

static void hero_mouse_release(HeroState *h) {
    EnableCursor();         /* rétablit un curseur visible et libre        */
    h->mouse_free = 1;
}

void hero_start(AppContext *ctx) {
    HeroState *h = &ctx->hero;
    GameState *gs = &ctx->gs;

    h->px = gs->units.base_px;                 /* spawn à la base primaire */
    h->py = gs->units.base_py + (float)TILE_SIZE * 1.5f;
    h->hz = 0.0f; h->vz = 0.0f; h->on_ground = 1;
    /* SPAWN SÛR : si la case visée est bloquée (eau/ruine/tour), cherche
       en spirale la tuile praticable la plus proche autour de la base. */
    if (hero_tile_blocked(gs, (int)(h->px / TILE_SIZE),
                          (int)(h->py / TILE_SIZE), 0.0f)) {
        int btx = (int)(gs->units.base_px / TILE_SIZE);
        int bty = (int)(gs->units.base_py / TILE_SIZE);
        int found = 0;
        for (int r = 1; r <= 6 && !found; r++) {
            for (int oy = -r; oy <= r && !found; oy++) {
                for (int ox = -r; ox <= r && !found; ox++) {
                    if (ox > -r && ox < r && oy > -r && oy < r) continue;
                    if (!hero_tile_blocked(gs, btx + ox, bty + oy, 0.0f)) {
                        h->px = (btx + ox + 0.5f) * (float)TILE_SIZE;
                        h->py = (bty + oy + 0.5f) * (float)TILE_SIZE;
                        found = 1;
                    }
                }
            }
        }
    }
    /* Regard initial : vers le centre de la carte (on voit le terrain). */
    {
        float mcx = gs->map.w * (float)TILE_SIZE * 0.5f;
        float mcy = gs->map.h * (float)TILE_SIZE * 0.5f;
        h->yaw = atan2f(mcx - h->px, mcy - h->py);
    }
    h->pitch = -0.10f;
    h->first_person = 1;                       /* 1re personne par défaut  */
    h->moving = 0; h->anim_t = 0.0f;

    h->weapon = HW_RIFLE;
    h->upg_dmg = 0; h->upg_rate = 0;
    h->fire_cd = 0.0f; h->fire_flash = 0.0f;
    h->trace_t = 0.0f; h->trace_hit = 0;

    h->place_mode = 0; h->place_type = 0;
    h->place_tx = 0; h->place_ty = 0; h->place_ok = 0;
    h->aim_on_target = 0;

    h->show_help = 0;
    h->toast_t = 0.0f; h->toast[0] = '\0';

    h->sprinting = 0;
    h->fov_cur   = HERO_CAM_FOVY;
    h->bob_t     = 0.0f;
    h->wave_banner_t = 0.0f; h->wave_banner[0] = '\0';
    h->prev_phase    = (int)gs->phase;

    /* OUVRIER OFFERT : sans lui, impossible de construire (le placement se
       fait via un ouvrier). On l'ajoute gratuitement à la base primaire. */
    {
        int wcost = UNIT_BASE_STATS[UNIT_WORKER].cost;
        gs->gold += wcost;                     /* net : ouvrier gratuit    */
        unit_spawn_at(&gs->units, UNIT_WORKER, &gs->gold, &gs->bonuses,
                      gs->units.base_px, gs->units.base_py);
    }

    gs->ui.speed_mult = 1;                     /* temps réel uniquement    */
    ctx->hero_mode = 1;

    /* PLEIN ÉCRAN IMPOSÉ (comme les vrais FPS) : en fenêtré, certains
       compositeurs (WSLg…) laissent le curseur fuir malgré la capture.
       En plein écran il n'a nulle part où aller. Restauré à la sortie. */
    h->fs_forced = 0;
    if (!IsWindowFullscreen()) {
        ToggleFullscreen();
        h->fs_forced = 1;
    }
    ctx->menu.opts.fullscreen = IsWindowFullscreen();

    hero_mouse_capture(h, ctx->menu.opts.hero_mouse_native);
    hero_toast(h, "Approche l'OUVRIER (repere vert) et appuie [E] pour batir",
               (Color){130, 230, 140, 255});
}

static void hero_exit_to_menu(AppContext *ctx) {
    EnableCursor();
    /* Restaure le fenêtré si le mode avait imposé le plein écran. */
    if (ctx->hero.fs_forced && IsWindowFullscreen()) {
        ToggleFullscreen();
        ctx->menu.opts.fullscreen = IsWindowFullscreen();
    }
    ctx->hero.fs_forced = 0;
    ctx->hero_mode   = 0;
    ctx->screen      = SCREEN_MENU;
    ctx->menu.screen = MENU_TITLE;
    ctx->menu.paused = 0;
    audio_play_menu_music();
}

/* ════════════════════════════════════════════════════════════════
   CAMÉRA — partagée entre rendu, tir et placement
   ════════════════════════════════════════════════════════════════ */
Camera3D hero_camera(const HeroState *h) {
    Vector3 feet = w3d_from_sim(h->px, h->py, h->hz);
    float cp = cosf(h->pitch), sp = sinf(h->pitch);
    Vector3 dir = { sinf(h->yaw) * cp, sp, cosf(h->yaw) * cp };

    Camera3D cam = {0};
    cam.up         = (Vector3){0.0f, 1.0f, 0.0f};
    cam.fovy       = (h->fov_cur > 1.0f) ? h->fov_cur : HERO_CAM_FOVY;
    cam.projection = CAMERA_PERSPECTIVE;
    if (h->first_person) {
        /* Balancement de marche (1re personne seulement, subtil). */
        float bob = h->moving ? sinf(h->bob_t) * HERO_BOB_AMP : 0.0f;
        Vector3 eye = { feet.x, feet.y + HERO_EYE_H + bob, feet.z };
        cam.position = eye;
        cam.target   = (Vector3){ eye.x + dir.x, eye.y + dir.y, eye.z + dir.z };
    } else {
        Vector3 head = { feet.x, feet.y + HERO_HEAD_H, feet.z };
        cam.position = (Vector3){ head.x - dir.x * HERO_CAM_DIST,
                                  head.y - dir.y * HERO_CAM_DIST + 0.9f,
                                  head.z - dir.z * HERO_CAM_DIST };
        /* Jamais sous le sol (regard vers le haut en 3e personne). */
        if (cam.position.y < 0.35f) cam.position.y = 0.35f;
        cam.target   = (Vector3){ head.x + dir.x * 2.0f,
                                  head.y + dir.y * 2.0f,
                                  head.z + dir.z * 2.0f };
    }
    return cam;
}

/* ════════════════════════════════════════════════════════════════
   DÉPLACEMENT + COLLISION (tuiles infranchissables + tours)
   ════════════════════════════════════════════════════════════════ */
/* Hauteur du sol (monde) sous une tuile : les ruines sont des blocs sur
   lesquels on peut grimper en sautant. */
static float hero_ground_h(const GameState *gs, int tx, int ty) {
    if (tx < 0 || ty < 0 || tx >= gs->map.w || ty >= gs->map.h) return 0.0f;
    if (gs->map.tiles[ty][tx].type == TILE_RUIN) return W3D_RUIN_H;
    return 0.0f;
}

/* Une tuile bloque-t-elle le héros À CETTE HAUTEUR de pieds ?
   L'eau et les tours bloquent toujours ; les ruines seulement si on est
   trop bas pour monter dessus (saut → franchissables). */
static int hero_tile_blocked(const GameState *gs, int tx, int ty, float feet_h) {
    if (tx < 0 || ty < 0 || tx >= gs->map.w || ty >= gs->map.h) return 1;
    const Tile *t = &gs->map.tiles[ty][tx];
    if (t->type == TILE_WATER) return 1;
    if (t->type == TILE_BASE)  return 1;   /* le bâtiment de base est solide */
    if (!t->passable && !(t->type == TILE_RUIN &&
                          feet_h + HERO_STEP_UP >= W3D_RUIN_H)) return 1;
    for (int i = 0; i < MAX_TOWERS; i++) {
        const Tower *tw = &gs->towers.towers[i];
        if (tw->active && tw->tile_x == tx && tw->tile_y == ty) return 1;
    }
    return 0;
}

/* Essaie un déplacement axe par axe (glissement le long des murs).
   ANTI-BLOCAGE : si la tuile COURANTE est déjà bloquée (spawn raté,
   tour posée sous soi…), tout mouvement est permis pour s'en extraire. */
static void hero_move(HeroState *h, const GameState *gs, float dx, float dy) {
    int stuck = hero_tile_blocked(gs, (int)(h->px / TILE_SIZE),
                                  (int)(h->py / TILE_SIZE), h->hz);

    float nx = h->px + dx;
    int etx = (int)((nx + (dx > 0.0f ? HERO_RADIUS_PX : -HERO_RADIUS_PX)) / TILE_SIZE);
    int ety = (int)(h->py / TILE_SIZE);
    if (stuck || !hero_tile_blocked(gs, etx, ety, h->hz)) h->px = nx;

    float ny = h->py + dy;
    int ftx = (int)(h->px / TILE_SIZE);
    int fty = (int)((ny + (dy > 0.0f ? HERO_RADIUS_PX : -HERO_RADIUS_PX)) / TILE_SIZE);
    if (stuck || !hero_tile_blocked(gs, ftx, fty, h->hz)) h->py = ny;

    /* Bornes dures de la carte (même « débloqué », on reste dedans). */
    float maxx = gs->map.w * (float)TILE_SIZE - HERO_RADIUS_PX;
    float maxy = gs->map.h * (float)TILE_SIZE - HERO_RADIUS_PX;
    if (h->px < HERO_RADIUS_PX) h->px = HERO_RADIUS_PX;
    if (h->py < HERO_RADIUS_PX) h->py = HERO_RADIUS_PX;
    if (h->px > maxx) h->px = maxx;
    if (h->py > maxy) h->py = maxy;
}

static void hero_input(AppContext *ctx, float dt) {
    HeroState *h = &ctx->hero;

    /* ── Regard SOURIS : écart au centre + recentrage manuel ────────
       Utilise virt_mouse (le chemin souris éprouvé des menus/HUD) :
       l'écran est recentré chaque frame, le delta = distance au centre
       du canvas. Anti-spike + frames de stabilisation après capture.  */
    int want_native = ctx->menu.opts.hero_mouse_native;
    if (IsKeyPressed(KEY_TAB)) {
        if (h->mouse_free) hero_mouse_capture(h, want_native);
        else               hero_mouse_release(h);
    }
    if (h->mouse_free && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        hero_mouse_capture(h, want_native);   /* clic = reprendre le contrôle */
    /* L'option a changé en cours de partie (via pause→Options) → resync. */
    if (!h->mouse_free && h->mouse_native != want_native)
        hero_mouse_capture(h, want_native);

    if (!h->mouse_free && IsWindowFocused()) {
        float dx, dy;
        if (h->mouse_native) {
            /* NATIF : delta relatif standard (curseur verrouillé).
               Ré-assertion du verrou à chaque frame (alt-tab, WSLg).
               SURTOUT PAS de SetMousePosition ici : forcer la position
               raylib au centre pendant que la position virtuelle GLFW
               continue ailleurs crée un delta géant constant (bug
               « je ne peux regarder que le sol »).                    */
            if (!IsCursorHidden()) DisableCursor();
            Vector2 md = GetMouseDelta();
            dx = md.x; dy = md.y;
        } else {
            /* STANDARD : écart au centre (position absolue, fiable partout)
               + recentrage. Le curseur — s'il reste visible malgré
               HideCursor (WSLg) — demeure ÉPINGLÉ au centre, sous le
               réticule, et le plein écran l'empêche de fuir. */
            if (!IsCursorHidden()) HideCursor();
            Vector2 vm = virt_mouse();
            dx = vm.x - (float)g_canvas_virt_w * 0.5f;
            dy = vm.y - (float)g_canvas_virt_h * 0.5f;
            SetMousePosition(GetScreenWidth() / 2, GetScreenHeight() / 2);
        }
        if (h->mouse_settle > 0) { h->mouse_settle--; dx = 0.0f; dy = 0.0f; }
        /* Anti-spike : un pic aberrant (alt-tab, glitch driver) ne fait
           jamais tourner de plus de HERO_LOOK_CLAMP en une frame. */
        if (dx >  HERO_LOOK_CLAMP) dx =  HERO_LOOK_CLAMP;
        if (dx < -HERO_LOOK_CLAMP) dx = -HERO_LOOK_CLAMP;
        if (dy >  HERO_LOOK_CLAMP) dy =  HERO_LOOK_CLAMP;
        if (dy < -HERO_LOOK_CLAMP) dy = -HERO_LOOK_CLAMP;

        /* Courbe d'accélération réglable : 50 = ×1.0 = LINÉAIRE (aucune
           accélération ajoutée) ; <50 amortit, >50 accentue les gestes
           rapides. delta' = signe · |delta|^pow.                        */
        float pow_k = (float)ctx->menu.opts.hero_accel / 50.0f;
        if (pow_k != 1.0f) {
            dx = (dx >= 0.0f ? 1.0f : -1.0f) * powf(fabsf(dx), pow_k);
            dy = (dy >= 0.0f ? 1.0f : -1.0f) * powf(fabsf(dy), pow_k);
        }
        /* Sensibilités par axe (50 = ×1, identiques par défaut). */
        float sx = HERO_MOUSE_SENS * ((float)ctx->menu.opts.hero_sens_x / 50.0f);
        float sy = HERO_MOUSE_SENS * ((float)ctx->menu.opts.hero_sens_y / 50.0f);
        float invy = ctx->menu.opts.hero_invert_y ? -1.0f : 1.0f;
        h->yaw   -= dx * sx;
        h->pitch -= dy * sy * invy;     /* norme : souris haut = regard haut */
    }

    /* ── Regard CLAVIER (secours toujours disponible) : flèches ───── */
    if (IsKeyDown(KEY_RIGHT)) h->yaw   -= HERO_KEY_TURN * dt;
    if (IsKeyDown(KEY_LEFT))  h->yaw   += HERO_KEY_TURN * dt;
    if (IsKeyDown(KEY_UP))    h->pitch += HERO_KEY_TURN * 0.6f * dt;
    if (IsKeyDown(KEY_DOWN))  h->pitch -= HERO_KEY_TURN * 0.6f * dt;

    if (h->pitch >  HERO_PITCH_MAX) h->pitch =  HERO_PITCH_MAX;
    if (h->pitch < -HERO_PITCH_MAX) h->pitch = -HERO_PITCH_MAX;

    /* Touches configurables (Options > Commandes). */
    const int *hk = ctx->menu.opts.hero_keys;

    /* Vue 1re/3e personne */
    if (IsKeyPressed(hk[HK_VIEW])) h->first_person ^= 1;
    if (IsKeyPressed(KEY_H)) h->show_help ^= 1;

    /* Déplacement relatif au cap caméra */
    float fwd = 0.0f, str = 0.0f;
    if (IsKeyDown(hk[HK_FWD]))   fwd += 1.0f;
    if (IsKeyDown(hk[HK_BACK]))  fwd -= 1.0f;
    if (IsKeyDown(hk[HK_RIGHT])) str += 1.0f;
    if (IsKeyDown(hk[HK_LEFT]))  str -= 1.0f;
    h->moving    = (fwd != 0.0f || str != 0.0f);
    h->sprinting = (h->moving && IsKeyDown(hk[HK_SPRINT]));
    if (h->moving) {
        float sp = HERO_SPEED_TILES * (float)TILE_SIZE;
        if (h->sprinting) sp *= HERO_SPRINT_MULT;
        /* base sim : avant = (sin yaw, cos yaw) ;
           DROITE CAMÉRA = (-cos yaw, sin yaw) — vérifié en jeu (l'inverse
           échange gauche/droite).                                        */
        float mx = (sinf(h->yaw) * fwd - cosf(h->yaw) * str);
        float my = (cosf(h->yaw) * fwd + sinf(h->yaw) * str);
        float ml = sqrtf(mx * mx + my * my);
        if (ml > 1e-4f) {
            hero_move(h, &ctx->gs, mx / ml * sp * dt, my / ml * sp * dt);
        }
    }

    /* ── Saut + gravité (les ruines deviennent franchissables) ────── */
    if (IsKeyPressed(hk[HK_JUMP]) && h->on_ground) {
        h->vz = HERO_JUMP_V;
        h->on_ground = 0;
    }
    {
        float gh = hero_ground_h(&ctx->gs, (int)(h->px / TILE_SIZE),
                                 (int)(h->py / TILE_SIZE));
        if (!h->on_ground || h->hz > gh) {
            h->vz -= HERO_GRAVITY * dt;
            h->hz += h->vz * dt;
        }
        if (h->hz <= gh) {              /* atterrissage / marche au sol */
            h->hz = gh;
            h->vz = 0.0f;
            h->on_ground = 1;
        } else {
            h->on_ground = 0;
        }
    }
    h->anim_t += dt;

    /* ── Feel caméra : FOV de sprint + balancement de marche ──────── */
    {
        float fov_target = h->sprinting ? HERO_FOV_SPRINT : HERO_CAM_FOVY;
        float k = HERO_FOV_LERP * dt;
        if (k > 1.0f) k = 1.0f;
        h->fov_cur += (fov_target - h->fov_cur) * k;
        if (h->moving && h->on_ground)
            h->bob_t += dt * HERO_BOB_FREQ * (h->sprinting ? 1.3f : 1.0f);
    }

    /* Interactions, tir, placement, achats — pas quand la souris est
       libérée (TAB) : un clic sert alors à reprendre le contrôle. */
    if (!h->mouse_free)
        hero_actions_update(ctx, dt);
    else
        h->aim_on_target = 0;
}

/* ════════════════════════════════════════════════════════════════
   (HUD, popups monde et panneaux : voir hero_hud.c)
   ════════════════════════════════════════════════════════════════ */
/* ════════════════════════════════════════════════════════════════
   FRAME COMPLÈTE
   ════════════════════════════════════════════════════════════════ */
int hero_frame(AppContext *ctx, float dt) {
    HeroState *h = &ctx->hero;
    GameState *gs = &ctx->gs;

    /* Option « compteur FPS » appliquée aussi en mode héros. */
    gs->ui.show_fps = ctx->menu.opts.show_fps;

    /* Timers d'affichage (tournent même en pause : inoffensif) */
    if (h->toast_t    > 0.0f) h->toast_t    -= dt;
    if (h->fire_flash > 0.0f) h->fire_flash -= dt;
    if (h->trace_t    > 0.0f) h->trace_t    -= dt;
    if (h->fire_cd    > 0.0f) h->fire_cd    -= dt;

    /* ── DÉFAITE ────────────────────────────────────────────────── */
    if (gs->phase == PHASE_GAMEOVER) {
        if (!h->mouse_free) hero_mouse_release(h);
        render3d_world_render(ctx, hero_camera(h));
        int y = hero_overlay_panel("DEFAITE", (Color){220, 70, 60, 255});
        char sb[64];
        snprintf(sb, sizeof(sb), "Vague %d  -  %d eliminations",
                 gs->wave_manager.number, gs->kills);
        int sw = mtxt(sb, 12);
        dtxt(sb, g_canvas_virt_w / 2 - sw / 2, y, 12, (Color){210, 200, 180, 255});
        const char *hint = "[ENTREE] ou [ESC]  retour au menu";
        int hw = mtxt(hint, 11);
        dtxt(hint, g_canvas_virt_w / 2 - hw / 2, y + fh(12) + 12, 11,
             (Color){170, 190, 210, 255});
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_ESCAPE))
            hero_exit_to_menu(ctx);
        return 1;
    }

    /* ── PAUSE = MENU CLASSIQUE (Options > Commandes accessibles) ── */
    if (ctx->menu.paused) {
        render3d_world_render(ctx, hero_camera(h));   /* le jeu en fond   */
        MenuAction pact = menu_render_and_act(&ctx->menu, &ctx->gs.meta,
                                              g_canvas_virt_w,
                                              g_canvas_virt_h);
        if (IsKeyPressed(KEY_ESCAPE)) {               /* ESC = reprendre  */
            ctx->menu.paused = 0;
            ctx->menu.screen = MENU_TITLE;
        }
        if (pact.save_and_quit == 1) {   /* retour menu (slot -1 : pas de save) */
            hero_exit_to_menu(ctx);
            return 1;
        }
        if (pact.quit_app) return 0;
        if (pact.toggle_fs == 1) {
            ToggleFullscreen();
            ctx->menu.opts.fullscreen = IsWindowFullscreen();
        }
        if (pact.toggle_fs == 2 && !IsWindowFullscreen())
            window_apply_size(ctx->menu.opts.win_width,
                              ctx->menu.opts.win_height);
        if (!ctx->menu.paused)          /* REPRENDRE (bouton ou ESC)      */
            hero_mouse_capture(h, ctx->menu.opts.hero_mouse_native);
        return 1;
    }

    /* ── JEU ────────────────────────────────────────────────────── */
    if (IsKeyPressed(KEY_ESCAPE)) {
        if (h->place_mode) {
            h->place_mode = 0;              /* ESC annule d'abord le placement */
        } else {
            ctx->menu.paused = 1;
            ctx->menu.screen = MENU_PAUSE;
            hero_mouse_release(h);
        }
    }

    hero_input(ctx, dt);
    game_state_update(gs, dt);              /* la MÊME sim que les autres modes */
    fx_update(dt);

    /* Bannière aux transitions de phase (VAGUE N / vague repoussée). */
    if (h->wave_banner_t > 0.0f) h->wave_banner_t -= dt;
    if ((int)gs->phase != h->prev_phase) {
        if (gs->phase == PHASE_WAVE) {
            snprintf(h->wave_banner, sizeof(h->wave_banner), "VAGUE %d",
                     gs->wave_manager.number);
            h->wave_banner_t = HERO_BANNER_TIME;
        } else if (gs->phase == PHASE_PREP &&
                   h->prev_phase == (int)PHASE_WAVE) {
            snprintf(h->wave_banner, sizeof(h->wave_banner),
                     "VAGUE %d REPOUSSEE !", gs->wave_manager.number);
            h->wave_banner_t = HERO_BANNER_TIME;
        }
        h->prev_phase = (int)gs->phase;
    }

    Camera3D cam = hero_camera(h);
    render3d_world_render(ctx, cam);
    hero_draw_world_popups(ctx, cam);       /* dégâts/or + barres PV ennemies   */
    hero_hud(ctx);
    return 1;
}
