/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  hero_hud.c ─ MODE HÉROS : couche 2D par-dessus la scène 3D.
 *  Viseur, bannière d'objectif, stats, arme, invites contextuelles,
 *  toast, aide [H], panneaux pause/défaite, et POPUPS DU MONDE
 *  (dégâts EFFICACE/RESISTE/SYNERGIE + or) projetés dans la vue 3D.
 *  Extrait de hero.c (règle projet : fichiers < 500 lignes).
 */
#include "hero.h"
#include "app.h"
#include "../ui/render3d_world.h"   /* w3d_from_sim                       */
#include "../ui/renderer.h"         /* g_canvas_virt_w/h                  */
#include "../ui/ui_utils.h"         /* dtxt / mtxt / fh                   */
#include "../combat/fx.h"           /* g_fx (popups à projeter)           */
#include "rlgl.h"                   /* depth test off (viewmodel)         */
#include <math.h>
#include <stdio.h>

#define HERO_POPUP_BASE_H  1.35f  /* hauteur monde de départ des popups   */
#define HERO_POPUP_RISE_W  1.0f   /* montée des popups (unités monde/s)   */
#define HERO_HPBAR_W       34     /* largeur barre PV ennemie (px canvas) */
#define HERO_HPBAR_BOSS_W  64     /* largeur barre PV de boss             */

/* ════════════════════════════════════════════════════════════════
   POPUPS DU MONDE (dégâts EFFICACE/RESISTE/SYNERGIE, or) EN VUE 3D
   Position sim → monde → canvas via GetWorldToScreenEx aux dimensions
   du CANVAS (= la projection exacte de la scène rendue dedans).
   ════════════════════════════════════════════════════════════════ */
void hero_draw_world_popups(AppContext *ctx, Camera3D cam) {
    Vector3 fw = { cam.target.x - cam.position.x,
                   cam.target.y - cam.position.y,
                   cam.target.z - cam.position.z };

    /* ── Barres de PV ennemies (entamés ou boss), projetées ──────── */
    {
        const GameState *gs = &ctx->gs;
        const HeroState *h  = &ctx->hero;
        float maxd = HERO_HPBAR_TILES * (float)TILE_SIZE;
        for (int i = 0; i < MAX_ENEMIES; i++) {
            const Enemy *e = &gs->enemies.enemies[i];
            if (!e->active || e->dead || e->spawn_delay > 0.0f) continue;
            if (!e->is_boss && e->hp >= e->max_hp) continue;  /* intacts : rien */
            float ex = e->x - h->px, ey = e->y - h->py;
            if (ex * ex + ey * ey > maxd * maxd) continue;    /* trop loin      */
            float top = W3D_ENEMY_BODY_H * 2.1f * (e->is_boss ? 1.8f : 1.0f);
            Vector3 p = w3d_from_sim(e->x, e->y, top);
            Vector3 d = { p.x - cam.position.x, p.y - cam.position.y,
                          p.z - cam.position.z };
            if (fw.x * d.x + fw.y * d.y + fw.z * d.z <= 0.0f) continue;
            Vector2 s = GetWorldToScreenEx(p, cam,
                                           g_canvas_virt_w, g_canvas_virt_h);
            float ratio = (e->max_hp > 0.0f) ? e->hp / e->max_hp : 0.0f;
            if (ratio < 0.0f) ratio = 0.0f;
            if (ratio > 1.0f) ratio = 1.0f;
            int bw = e->is_boss ? HERO_HPBAR_BOSS_W : HERO_HPBAR_W;
            int bx = (int)s.x - bw / 2, by = (int)s.y;
            Color fc = (ratio > 0.55f) ? (Color){ 90, 210,  90, 235}
                     : (ratio > 0.25f) ? (Color){235, 190,  60, 235}
                                       : (Color){225,  70,  55, 235};
            DrawRectangle(bx - 1, by - 1, bw + 2, 6, (Color){10, 10, 12, 200});
            DrawRectangle(bx, by, (int)(bw * ratio), 4, fc);
        }
    }

    for (int i = 0; i < FX_MAX_POPUPS; i++) {
        const FxPopup *u = &g_fx.popups[i];
        if (!u->active) continue;
        float el = u->max_life - u->life;      /* temps écoulé (s)        */
        /* Le popup 2D monte en décalant son y ; on reconstruit le point
           d'origine AU SOL et on convertit la montée en hauteur monde. */
        float y0  = u->y - u->vy * el;
        Vector3 p = w3d_from_sim(u->x, y0,
                                 HERO_POPUP_BASE_H + el * HERO_POPUP_RISE_W);
        Vector3 d = { p.x - cam.position.x, p.y - cam.position.y,
                      p.z - cam.position.z };
        if (fw.x * d.x + fw.y * d.y + fw.z * d.z <= 0.0f) continue; /* dos */
        Vector2 s = GetWorldToScreenEx(p, cam,
                                       g_canvas_virt_w, g_canvas_virt_h);
        float a = (u->max_life > 0.0f) ? u->life / u->max_life : 0.0f;
        if (a < 0.0f) a = 0.0f;
        if (a > 1.0f) a = 1.0f;
        Color c = u->col;
        c.a = (unsigned char)(255.0f * a);
        int w = mtxt(u->text, 11);
        dtxt(u->text, (int)s.x - w / 2, (int)s.y, 11, c);
    }
}

/* ════════════════════════════════════════════════════════════════
   HUD 2D (par-dessus la scène 3D, dans le canvas)
   ════════════════════════════════════════════════════════════════ */
void hero_hud(AppContext *ctx) {
    HeroState *h = &ctx->hero;
    GameState *gs = &ctx->gs;
    int cx = g_canvas_virt_w / 2, cy = g_canvas_virt_h / 2;

    /* Viseur RÉACTIF : blanc au repos ; ROUGE + écarté sur une cible ;
       flash orange au tir. */
    int on = h->aim_on_target;
    Color cc = (h->fire_flash > 0.0f) ? (Color){255, 170, 80, 255}
             : on                     ? (Color){255,  85, 60, 255}
                                      : (Color){235, 235, 235, 210};
    int g1 = on ? 5 : 3, g2 = on ? 13 : 9;   /* écartement sur cible */
    DrawLine(cx - g2, cy, cx - g1, cy, cc);
    DrawLine(cx + g1, cy, cx + g2, cy, cc);
    DrawLine(cx, cy - g2, cx, cy - g1, cc);
    DrawLine(cx, cy + g1, cx, cy + g2, cc);
    if (on) DrawCircleLines(cx, cy, 3, cc);  /* point central sur cible */

    /* Hit-marker : 4 encoches diagonales quand le tir a touché */
    if (h->trace_t > 0.0f && h->trace_hit) {
        unsigned char a =
            (unsigned char)(255.0f * (h->trace_t / HERO_TRACE_TIME));
        Color hm = {255, 130, 90, a};
        DrawLine(cx - 12, cy - 12, cx - 6, cy - 6, hm);
        DrawLine(cx + 6,  cy - 6,  cx + 12, cy - 12, hm);
        DrawLine(cx - 12, cy + 12, cx - 6, cy + 6, hm);
        DrawLine(cx + 6,  cy + 6,  cx + 12, cy + 12, hm);
    }

    /* ── Bannière de VAGUE (transitions de phase) ─────────────────── */
    if (h->wave_banner_t > 0.0f) {
        float a = h->wave_banner_t / HERO_BANNER_TIME;
        if (a > 1.0f) a = 1.0f;
        int bw2 = mtxt(h->wave_banner, 24);
        Color bc = {245, 200, 90, (unsigned char)(255.0f * a)};
        dtxt(h->wave_banner, cx - bw2 / 2, cy - 130, 24, bc);
    }

    /* ── MINIMAP (bas-droit) : la conscience tactique du TD ──────────
       Carte réduite + tours/unités/ennemis/héros en temps réel.       */
    {
        const Map *map = &gs->map;
        int   mw = HERO_MMAP_W;
        float sc = (float)mw / (float)(map->w * TILE_SIZE);
        int   mh = (int)(map->h * (float)TILE_SIZE * sc);
        int   mx = g_canvas_virt_w - mw - 10;
        int   my = g_canvas_virt_h - mh - 10;
        int   ts = (int)((float)TILE_SIZE * sc) + 1;   /* taille tuile mini */

        DrawRectangle(mx - 3, my - 3, mw + 6, mh + 6, (Color){8, 10, 14, 205});
        DrawRectangleLines(mx - 3, my - 3, mw + 6, mh + 6,
                           (Color){90, 110, 130, 220});
        for (int ty = 0; ty < map->h; ty++) {
            for (int tx = 0; tx < map->w; tx++) {
                Color c;
                switch (map->tiles[ty][tx].type) {
                    case TILE_PATH:  c = (Color){ 96,  76,  52, 255}; break;
                    case TILE_WATER: c = (Color){ 30,  55,  92, 255}; break;
                    case TILE_RUIN:  c = (Color){ 72,  72,  78, 255}; break;
                    case TILE_SPAWN: c = (Color){ 96,  40,  40, 255}; break;
                    case TILE_BASE:  c = (Color){ 52,  84,  56, 255}; break;
                    default:         c = (Color){ 38,  40,  36, 255}; break;
                }
                DrawRectangle(mx + (int)(tx * (float)TILE_SIZE * sc),
                              my + (int)(ty * (float)TILE_SIZE * sc), ts, ts, c);
            }
        }
        for (int d = 0; d < map->deposit_count; d++) {   /* minerais */
            const MaterialDeposit *dep = &map->deposits[d];
            if (!dep->active) continue;
            DrawRectangle(mx + (int)(dep->tile_x * (float)TILE_SIZE * sc),
                          my + (int)(dep->tile_y * (float)TILE_SIZE * sc),
                          ts, ts, MATERIAL_COLORS[dep->type]);
        }
        for (int i = 0; i < MAX_TOWERS; i++) {           /* tours */
            const Tower *tw = &gs->towers.towers[i];
            if (!tw->active) continue;
            DrawRectangle(mx + (int)(tw->cx * sc) - 1,
                          my + (int)(tw->cy * sc) - 1, 3, 3,
                          renderer_tower_color(tw->type));
        }
        for (int i = 0; i < MAX_UNITS; i++) {            /* unités */
            const Unit *u = &gs->units.units[i];
            if (!u->active) continue;
            DrawRectangle(mx + (int)(u->x * sc) - 1,
                          my + (int)(u->y * sc) - 1, 2, 2,
                          renderer_unit_color(u->type));
        }
        for (int i = 0; i < MAX_ENEMIES; i++) {          /* ennemis */
            const Enemy *e = &gs->enemies.enemies[i];
            if (!e->active || e->dead || e->spawn_delay > 0.0f) continue;
            int sz = e->is_boss ? 4 : 2;
            DrawRectangle(mx + (int)(e->x * sc) - sz / 2,
                          my + (int)(e->y * sc) - sz / 2, sz, sz,
                          renderer_enemy_color(e->type));
        }
        {   /* héros : point blanc + direction du regard */
            int hx = mx + (int)(h->px * sc), hy = my + (int)(h->py * sc);
            DrawRectangle(hx - 1, hy - 1, 3, 3, (Color){250, 250, 250, 255});
            DrawLine(hx, hy, hx + (int)(sinf(h->yaw) * 7.0f),
                     hy + (int)(cosf(h->yaw) * 7.0f),
                     (Color){250, 250, 250, 220});
        }
    }

    /* ── BANNIÈRE D'OBJECTIF (haut-centre) : guide tant qu'aucune tour ──
       n'est posée. Le placement passe par un ouvrier (repère vert). */
    if (!h->place_mode && gs->towers.tower_count == 0) {
        char kn[24];
        opts_key_name(ctx->menu.opts.hero_keys[HK_INTERACT], kn, sizeof(kn));
        char ob[96];
        snprintf(ob, sizeof(ob),
                 "OBJECTIF : rejoins l'OUVRIER (repere vert) puis [%s] pour batir une tour",
                 kn);
        int ow = mtxt(ob, 11);
        float pulse = (sinf((float)GetTime() * 3.0f) + 1.0f) * 0.5f;
        DrawRectangle(cx - ow/2 - 12, 40, ow + 24, fh(11) + 12,
                      (Color){10, 26, 12, 220});
        DrawRectangleLines(cx - ow/2 - 12, 40, ow + 24, fh(11) + 12,
                           (Color){70, 200, 90, (unsigned char)(160 + pulse*90)});
        dtxt(ob, cx - ow/2, 46, 11, (Color){150, 240, 160, 255});
    }

    /* Stats (haut-gauche) */
    char l1[64], l2[64];
    snprintf(l1, sizeof(l1), "OR %d    VIES %d", gs->gold, gs->lives);
    if (gs->phase == PHASE_PREP) {
        snprintf(l2, sizeof(l2), "VAGUE %d  -  prepa %.0fs",
                 gs->wave_manager.number + 1, gs->wave_manager.prep_timer);
    } else {
        snprintf(l2, sizeof(l2), "VAGUE %d  -  ennemis %d",
                 gs->wave_manager.number, enemy_pool_alive(&gs->enemies));
    }
    DrawRectangle(8, 8, mtxt(l1, 12) > mtxt(l2, 10) ? mtxt(l1, 12) + 16 : mtxt(l2, 10) + 16,
                  fh(12) + fh(10) + 14, (Color){8, 10, 16, 190});
    dtxt(l1, 16, 12, 12, (Color){240, 210, 120, 255});
    dtxt(l2, 16, 12 + fh(12) + 4, 10, (Color){200, 210, 225, 255});
    /* V1 : le héros n'est pas ciblable — assumé et affiché. */
    dtxt("HEROS INVULNERABLE (beta)", 16, 8 + fh(12) + fh(10) + 14 + 3, 8,
         (Color){140, 155, 175, 190});

    /* Arme (haut-droit) */
    char w1[80];
    snprintf(w1, sizeof(w1), "%s  [dgts %.0f | %.1f/s]  amel. %d/%d",
             hero_weapon_name(h->weapon), hero_weapon_dmg(h),
             hero_weapon_rate(h), h->upg_dmg + h->upg_rate, HERO_UPG_MAX * 2);
    int ww = mtxt(w1, 10);
    DrawRectangle(g_canvas_virt_w - ww - 24, 8, ww + 16, fh(10) + 10,
                  (Color){8, 10, 16, 190});
    dtxt(w1, g_canvas_virt_w - ww - 16, 13, 10, (Color){170, 220, 255, 255});

    /* Invites contextuelles (bas-centre) */
    char lines[6][64];
    int nl = hero_prompts(ctx, lines, 6);
    int py = g_canvas_virt_h - 30 - nl * (fh(10) + 4);
    for (int i = 0; i < nl; i++) {
        int lw = mtxt(lines[i], 10);
        DrawRectangle(cx - lw / 2 - 8, py - 2, lw + 16, fh(10) + 5,
                      (Color){8, 10, 16, 170});
        dtxt(lines[i], cx - lw / 2, py, 10, (Color){215, 225, 235, 255});
        py += fh(10) + 4;
    }

    /* Toast */
    if (h->toast_t > 0.0f) {
        int tw = mtxt(h->toast, 11);
        dtxt(h->toast, cx - tw / 2, g_canvas_virt_h - 30 - (nl + 1) * (fh(10) + 4) - fh(11),
             11, h->toast_col);
    }

    /* Aide (H) */
    if (h->show_help) {
        static const char *HL[] = {
            "Deplacement / SAUT / sprint / interactions :",
            "  touches configurables dans OPTIONS > COMMANDES",
            "SOURIS     viser            CLIC G tirer",
            "FLECHES    tourner la vue (secours clavier)",
            "TAB        liberer / capturer la souris",
            "1-5        (pres d'une BASE) recruter une unite",
            "O / P      (base) ameliorer degats / cadence",
            "ESC        pause    H  fermer l'aide",
        };
        int n = (int)(sizeof(HL) / sizeof(HL[0]));
        int pw = 480, ph = 20 + n * (fh(10) + 4) + 10;
        int px = cx - pw / 2, py2 = cy - ph / 2;
        DrawRectangle(px, py2, pw, ph, (Color){6, 10, 18, 235});
        DrawRectangleLines(px, py2, pw, ph, (Color){90, 150, 210, 235});
        int ly = py2 + 12;
        for (int i = 0; i < n; i++) {
            dtxt(HL[i], px + 20, ly, 10, (Color){210, 220, 232, 255});
            ly += fh(10) + 4;
        }
    }

    if (gs->ui.show_fps) {
        char fb[16];
        snprintf(fb, sizeof(fb), "%d FPS", GetFPS());
        dtxt(fb, 12, g_canvas_virt_h - fh(10) - 8, 10, (Color){120, 220, 120, 220});
    }
}

/* Voile + panneau centré (pause / défaite) ; retourne le y du contenu. */
int hero_overlay_panel(const char *title, Color col) {
    int cx = g_canvas_virt_w / 2;
    DrawRectangle(0, 0, g_canvas_virt_w, g_canvas_virt_h, (Color){0, 0, 0, 170});
    int tw = mtxt(title, 30);
    dtxt(title, cx - tw / 2, g_canvas_virt_h / 2 - 70, 30, col);
    return g_canvas_virt_h / 2 - 70 + fh(30) + 14;
}

/* ════════════════════════════════════════════════════════════════
   VIEWMODEL — l'arme en 1re personne (silhouette par archétype,
   recul au tir, flash de bouche). Dessiné DANS la scène 3D, en
   dernier, hors depth-test (ne clippe jamais dans le décor).
   ════════════════════════════════════════════════════════════════ */
#define HERO_VM_FWD    0.52f   /* distance devant la caméra (monde)     */
#define HERO_VM_RIGHT  0.26f   /* décalage à droite                     */
#define HERO_VM_DOWN   0.20f   /* décalage vers le bas                  */
#define HERO_VM_RECOIL 0.10f   /* recul max au tir (monde)              */

void hero_draw_viewmodel(const HeroState *h, Camera3D cam) {
    if (!h->first_person) return;
    if (h->place_mode) return;     /* arme baissée pendant la construction */

    Vector3 f = { cam.target.x - cam.position.x,
                  cam.target.y - cam.position.y,
                  cam.target.z - cam.position.z };
    float fl = sqrtf(f.x * f.x + f.y * f.y + f.z * f.z);
    if (fl < 1e-5f) return;
    f.x /= fl; f.y /= fl; f.z /= fl;
    /* droite = fwd × up(0,1,0) (normalisée dans le plan sol) */
    Vector3 r = { -f.z, 0.0f, f.x };
    float rl = sqrtf(r.x * r.x + r.z * r.z);
    if (rl < 1e-5f) return;
    r.x /= rl; r.z /= rl;

    float recoil = (h->fire_flash > 0.0f) ? h->fire_flash * HERO_VM_RECOIL * 8.0f
                                          : 0.0f;
    float bob = h->moving ? sinf(h->bob_t) * 0.012f : 0.0f;
    Vector3 base = {
        cam.position.x + f.x * (HERO_VM_FWD - recoil) + r.x * HERO_VM_RIGHT,
        cam.position.y + f.y * (HERO_VM_FWD - recoil) - HERO_VM_DOWN + bob,
        cam.position.z + f.z * (HERO_VM_FWD - recoil) + r.z * HERO_VM_RIGHT,
    };
    float yaw_deg = atan2f(f.x, f.z) * RAD2DEG;
    float elev    = asinf(f.y > 1.0f ? 1.0f : (f.y < -1.0f ? -1.0f : f.y));

    rlDisableDepthTest();
    rlPushMatrix();
        rlTranslatef(base.x, base.y, base.z);
        rlRotatef(yaw_deg, 0.0f, 1.0f, 0.0f);
        rlRotatef(-elev * RAD2DEG, 1.0f, 0.0f, 0.0f);
        /* Repère local : +Z = devant. Silhouette par archétype. */
        switch (h->weapon) {
            case HW_CANNON:   /* canon : fût massif + gueule large      */
                DrawCube((Vector3){0, -0.01f, 0.10f}, 0.10f, 0.10f, 0.30f,
                         (Color){96, 66, 44, 255});
                DrawCylinderEx((Vector3){0, 0, 0.20f}, (Vector3){0, 0, 0.46f},
                               0.055f, 0.065f, 10, (Color){70, 52, 38, 255});
                DrawCylinderEx((Vector3){0, 0, 0.44f}, (Vector3){0, 0, 0.47f},
                               0.075f, 0.075f, 10, (Color){50, 38, 28, 255});
                break;
            case HW_TESLA:    /* arc tesla : corps + bobine + orbe      */
                DrawCube((Vector3){0, -0.01f, 0.10f}, 0.07f, 0.08f, 0.26f,
                         (Color){44, 62, 82, 255});
                DrawCylinderEx((Vector3){0, 0, 0.20f}, (Vector3){0, 0, 0.38f},
                               0.045f, 0.03f, 8, (Color){70, 95, 120, 255});
                DrawSphere((Vector3){0, 0, 0.40f}, 0.035f,
                           (Color){120, 210, 245, 255});
                break;
            default:          /* fusil : corps fin + canon long + crosse */
                DrawCube((Vector3){0, -0.015f, 0.06f}, 0.05f, 0.07f, 0.22f,
                         (Color){68, 72, 82, 255});
                DrawCylinderEx((Vector3){0, 0.01f, 0.16f},
                               (Vector3){0, 0.01f, 0.50f},
                               0.018f, 0.018f, 8, (Color){52, 56, 64, 255});
                DrawCube((Vector3){0, -0.05f, -0.05f}, 0.045f, 0.08f, 0.09f,
                         (Color){84, 62, 40, 255});
                break;
        }
        /* Flash de bouche bref */
        if (h->fire_flash > 0.06f)
            DrawSphere((Vector3){0, 0, 0.52f}, 0.045f,
                       (Color){255, 215, 130, 240});
    rlPopMatrix();
    rlEnableDepthTest();
}
