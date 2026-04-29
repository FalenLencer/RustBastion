// renderer.c
#include "renderer.h"
#include "raylib.h"
#include "pathfinding.h"
#include "map_gen.h"
#include "theme.h"
#include "enemy.h"
#include "tower.h"
#include <math.h>
#include "unit.h"
#include "game_state.h"
#include "ui.h"

// ── Macros de scaling ─────────────────────────────────────────
#define S(x) ((int)((x) * RENDER_SCALE))
#define SF(x) ((float)(x) * RENDER_SCALE)

// ── Couleurs des tours, unités et projectiles ─────────────────
// Sans static → accessibles via extern dans renderer.h
Color TOWER_FILL[TOWER_TYPE_COUNT] = {
    [TOWER_GUN]    = {192, 57,  43,  255},
    [TOWER_SNIPER] = { 52,152, 219,  255},
    [TOWER_FLAME]  = {230,126,  34,  255},
    [TOWER_TESLA]  = {155, 89, 182,  255},
};
Color UNIT_FILL[UNIT_TYPE_COUNT] = {
    [UNIT_SOLDIER] = { 39,174,  96,  255},
    [UNIT_HEAVY]   = { 41,128, 185,  255},
    [UNIT_MEDIC]   = {231, 76,  60,  255},
    [UNIT_DOG]     = {243,156,  18,  255},
};
Color PROJ_COLOR[TOWER_TYPE_COUNT] = {
    [TOWER_GUN]    = {255,128,128,  255},
    [TOWER_SNIPER] = {135,206,235,  255},
    [TOWER_FLAME]  = {255,107, 53,  255},
    [TOWER_TESLA]  = {218,112,214,  255},
};

// ── Scaling global pour adaptation fenêtre ───────────────────
float RENDER_SCALE = 1.0f;

Color renderer_tower_color(TowerType type) { return TOWER_FILL[type]; }
Color renderer_unit_color (UnitType  type) { return UNIT_FILL[type];  }

// ── Une couleur ocre distincte par chemin ─────────────────────
static Color PATH_COLORS[MAX_PATHS] = {
    {239, 159,  39, 160},   // chemin 0 — ocre
    { 39, 159, 239, 160},   // chemin 1 — bleu acier
    {159, 239,  39, 160},   // chemin 2 — vert radioactif
};

// ════════════════════════════════════════════════════
// DÉTAILS PIXEL ART PAR THÈME
// ════════════════════════════════════════════════════
void render_tile_detail(int px, int py, TileType type, ThemeID theme) {
    switch (theme) {
        case THEME_WASTELAND:
            if (type==TILE_RUIN)  { DrawRectangle(px+4,py+4,6,4,(Color){90,65,40,180}); DrawRectangle(px+14,py+10,8,3,(Color){90,65,40,150}); }
            if (type==TILE_GROUND){ DrawRectangle(px+8,py+6,3,2,(Color){55,38,15,200}); DrawRectangle(px+20,py+18,4,2,(Color){55,38,15,180}); }
            if (type==TILE_WATER) { DrawRectangle(px+3,py+12,TILE_SIZE-6,2,(Color){26,90,53,160}); }
            if (type==TILE_PATH)  { for(int i=0;i<3;i++) DrawRectangle(px+4+i*10,py+TILE_SIZE/2-1,7,2,(Color){122,82,0,200}); }
            break;
        case THEME_SWAMP:
            if (type==TILE_RUIN)  { DrawRectangle(px+3,py+3,4,TILE_SIZE-6,(Color){61,92,48,180}); DrawRectangle(px+10,py+5,3,TILE_SIZE-8,(Color){61,92,48,150}); DrawRectangle(px+18,py+2,4,TILE_SIZE-4,(Color){61,92,48,130}); }
            if (type==TILE_WATER) { DrawRectangle(px+2,py+8,TILE_SIZE-4,3,(Color){45,107,45,160}); DrawRectangle(px+4,py+18,TILE_SIZE-8,2,(Color){26,74,26,140}); }
            if (type==TILE_PATH)  { for(int i=0;i<4;i++) DrawRectangle(px+2+i*7,py+TILE_SIZE/2-1,5,2,(Color){61,163,91,200}); }
            break;
        case THEME_DESERT:
            if (type==TILE_RUIN)  { DrawRectangle(px+6,py+8,TILE_SIZE-12,4,(Color){139,94,42,200}); DrawRectangle(px+8,py+4,4,TILE_SIZE-8,(Color){139,94,42,180}); }
            if (type==TILE_GROUND){ DrawRectangle(px+4,py+10,TILE_SIZE-8,1,(Color){107,74,32,160}); DrawRectangle(px+6,py+18,TILE_SIZE-12,1,(Color){107,74,32,140}); }
            if (type==TILE_PATH)  { DrawRectangle(px+4,py+TILE_SIZE/2-2,TILE_SIZE-8,4,(Color){196,150,42,220}); }
            break;
        case THEME_CITY:
            if (type==TILE_RUIN)  { DrawRectangle(px+2,py+2,TILE_SIZE-4,TILE_SIZE-4,(Color){92,92,92,200}); DrawRectangle(px+4,py+4,6,8,(Color){42,42,42,255}); DrawRectangle(px+13,py+4,7,8,(Color){42,42,42,255}); DrawRectangle(px+4,py+15,6,6,(Color){42,42,42,255}); }
            if (type==TILE_GROUND){ DrawLine(px,py+TILE_SIZE/2,px+TILE_SIZE,py+TILE_SIZE/2,(Color){61,61,61,200}); DrawLine(px+TILE_SIZE/2,py,px+TILE_SIZE/2,py+TILE_SIZE,(Color){61,61,61,200}); }
            if (type==TILE_PATH)  { DrawRectangle(px+2,py+2,TILE_SIZE-4,TILE_SIZE-4,(Color){58,58,58,200}); DrawRectangle(px+TILE_SIZE/2-1,py+2,1,8,(Color){255,255,0,180}); DrawRectangle(px+TILE_SIZE/2-1,py+TILE_SIZE-10,1,8,(Color){255,255,0,180}); }
            break;
        case THEME_FACTORY:
            if (type==TILE_RUIN)  { DrawRectangle(px+4,py+4,TILE_SIZE-8,TILE_SIZE-8,(Color){107,58,26,200}); DrawRectangle(px+8,py+8,TILE_SIZE-16,TILE_SIZE-16,(Color){45,26,10,255}); DrawRectangle(px+6,py+TILE_SIZE/2-1,TILE_SIZE-12,2,(Color){139,69,19,200}); }
            if (type==TILE_GROUND){ for(int i=0;i<3;i++) DrawLine(px+i*(TILE_SIZE/3),py,px+i*(TILE_SIZE/3),py+TILE_SIZE,(Color){45,30,26,200}); }
            if (type==TILE_PATH)  { DrawRectangle(px+2,py+2,TILE_SIZE-4,TILE_SIZE-4,(Color){74,42,10,200}); for(int i=0;i<4;i++) DrawRectangle(px+4+i*6,py+TILE_SIZE/2-2,4,4,(Color){139,90,26,220}); }
            break;
        default: break;
    }
}

// ════════════════════════════════════════════════════
// CARTE
// ════════════════════════════════════════════════════
void render_map(const Map *map) {
    const Theme *th = theme_get(map->theme);
    const ThemePalette *p = &th->palette;

    for (int y = 0; y < MAP_H; y++) {
        for (int x = 0; x < MAP_W; x++) {
            TileType t  = map->tiles[y][x].type;
            int      px = x * TILE_SIZE;
            int      py = y * TILE_SIZE;

            Color fill, stroke;
            switch (t) {
                case TILE_GROUND: fill=p->ground_fill; stroke=p->ground_stroke; break;
                case TILE_RUIN:   fill=p->ruin_fill;   stroke=p->ruin_stroke;   break;
                case TILE_WATER:  fill=p->water_fill;  stroke=p->water_stroke;  break;
                case TILE_PATH:   fill=p->path_fill;   stroke=p->path_stroke;   break;
                case TILE_SPAWN:  fill=p->spawn_fill;  stroke=p->spawn_stroke;  break;
                case TILE_BASE:   fill=p->base_fill;   stroke=p->base_stroke;   break;
                default:          fill=p->ground_fill; stroke=p->ground_stroke; break;
            }

            DrawRectangle(px, py, TILE_SIZE, TILE_SIZE, fill);
            DrawRectangleLines(px, py, TILE_SIZE, TILE_SIZE, stroke);
            render_tile_detail(px, py, t, map->theme);
        }
    }
}

// ════════════════════════════════════════════════════
// HUD IN-GAME
// ════════════════════════════════════════════════════
void render_hud(const GameState *gs) {
    const Theme *th = theme_get(gs->map.theme);
    int sw    = GetScreenWidth();
    int hud_y = (int)(MAP_H * TILE_SIZE * RENDER_SCALE);
    int line  = S(22);
    int pad   = S(6);

    DrawRectangle(0, hud_y, sw, GetScreenHeight() - hud_y,
                  (Color){12, 8, 4, 255});
    DrawLine(0, hud_y, sw, hud_y, (Color){139, 94, 0, 255});

    int c1 = S(10);
    int c2 = S(220);
    int c3 = S(430);
    int c4 = S(680);

    // COL 1 : infos partie
    int y = hud_y + pad;
    DrawText(TextFormat("OR    : %d",  gs->gold),  c1, y, S(18), (Color){239,159,39,255}); y += line;
    DrawText(TextFormat("VIES  : %d",  gs->lives), c1, y, S(18), (Color){231,76,60,255});  y += line;
    DrawText(TextFormat("VAGUE : %d",  gs->wave_manager.number), c1, y, S(18), (Color){180,180,160,255}); y += line;
    DrawText(gs->phase == PHASE_PREP ? "PREPARATION" :
             gs->phase == PHASE_WAVE ? "VAGUE EN COURS" : "GAME OVER",
             c1, y, S(16), (Color){160,160,140,255});

    // COL 2 : carte + timer
    y = hud_y + pad;
    DrawText(TextFormat("THEME : %s", th->name),    c2, y, S(18), (Color){180,220,180,255}); y += line;
    DrawText(TextFormat("SEED  : %d", gs->map.seed),c2, y, S(18), (Color){100,100,80,255});  y += line;
    DrawText(TextFormat("VOIES : %d", gs->map.path_count), c2, y, S(18), (Color){100,100,80,255}); y += line;
    if (gs->phase == PHASE_PREP)
        DrawText(TextFormat("Prochain: %.0fs", gs->wave_manager.prep_timer),
                 c2, y, S(18), (Color){239,159,39,255});
    else
        DrawText(TextFormat("Ennemis: %d", enemy_pool_alive(&gs->enemies)),
                 c2, y, S(18), (Color){231,76,60,255});

    // COL 3 : mode placement
    y = hud_y + pad;
    const char *mode_label = ui_tool_is_unit(gs->ui.selected_tool)
                             ? "MODE : UNITE" : "MODE : TOUR";
    Color mode_col = ui_tool_is_unit(gs->ui.selected_tool)
                     ? (Color){39,174,96,255} : (Color){239,159,39,255};
    DrawText(mode_label, c3, y, S(16), mode_col); y += line;
    DrawText("Clic droit = annuler", c3, y, S(13), (Color){100,100,80,255}); y += line;
    if (gs->ui.selected_tool != TOOL_NONE) {
        Color col = ui_tool_is_unit(gs->ui.selected_tool)
                    ? renderer_unit_color(ui_tool_to_unit(gs->ui.selected_tool))
                    : renderer_tower_color(ui_tool_to_tower(gs->ui.selected_tool));
        DrawText(TextFormat("Selectionne : %s", ui_tool_name(gs->ui.selected_tool)),
                 c3, y, S(14), col); y += line;
    } else {
        DrawText("Rien selectionne", c3, y, S(13), (Color){80,70,50,255}); y += line;
    }
    DrawText("1-4:Tours  5-8:Unites", c3, y, S(12), (Color){80,70,50,255});

    // COL 4 : chemins
    y = hud_y + pad;
    static Color path_colors[MAX_PATHS] = {
        {239,159,39,255}, {39,159,239,255}, {159,239,39,255},
    };
    const char *edge_names[] = {"GAUCHE","DROITE","HAUT","BAS"};
    for (int i = 0; i < gs->map.path_count; i++) {
        if (!gs->map.paths[i].active) continue;
        DrawText(TextFormat("Voie %d : %s", i, edge_names[gs->map.paths[i].spawn_edge]),
                 c4, y, S(16), path_colors[i % MAX_PATHS]);
        y += line;
    }
    if (gs->map.path_count > 0)
        DrawText(TextFormat("Base : (%d,%d)",
                     gs->map.paths[0].base.x, gs->map.paths[0].base.y),
                 c4, y, S(16), (Color){46,204,113,255});
}

// ════════════════════════════════════════════════════
// CHEMINS A*
// ════════════════════════════════════════════════════
void render_paths(const PathSet *ps) {
    for (int p = 0; p < ps->count; p++) {
        const Path *path = &ps->paths[p];
        if (!path->found) continue;
        Color col = PATH_COLORS[path->path_id % MAX_PATHS];
        for (int i = 0; i < path->len - 1; i++) {
            DrawLineEx(
                (Vector2){path->steps[i].x  *TILE_SIZE + TILE_SIZE/2.0f,
                          path->steps[i].y  *TILE_SIZE + TILE_SIZE/2.0f},
                (Vector2){path->steps[i+1].x*TILE_SIZE + TILE_SIZE/2.0f,
                          path->steps[i+1].y*TILE_SIZE + TILE_SIZE/2.0f},
                2.5f, col
            );
            DrawCircle(path->steps[i].x*TILE_SIZE+TILE_SIZE/2,
                       path->steps[i].y*TILE_SIZE+TILE_SIZE/2,
                       3, col);
        }
    }
}

// ════════════════════════════════════════════════════
// ENNEMIS
// ════════════════════════════════════════════════════
void render_enemies(const EnemyPool *pool) {
    for (int i = 0; i < MAX_ENEMIES; i++) {
        const Enemy *e = &pool->enemies[i];
        if (!e->active || e->dead || e->spawn_delay > 0.0f) continue;

        // Ombre
        DrawEllipse((int)e->x + 2, (int)e->y + (int)e->size,
                    (int)(e->size * 0.8f), (int)(e->size * 0.3f),
                    (Color){0,0,0,80});

        // Corps
        Color col;
        switch (e->type) {
            case ENEMY_RAIDER:  col = (Color){231, 76,  60,  255}; break;
            case ENEMY_BRUTE:   col = (Color){230, 126, 34,  255}; break;
            case ENEMY_RUNNER:  col = (Color){243, 156, 18,  255}; break;
            case ENEMY_VEHICLE: col = (Color){127, 140, 141, 255}; break;
            case ENEMY_MUTANT:  col = (Color){39,  174, 96,  255}; break;
            default:            col = WHITE;
        }
        if (e->slow_timer > 0.0f)
            col = (Color){100, 180, 255, 255};

        DrawCircle((int)e->x, (int)e->y, e->size, col);
        DrawCircleLines((int)e->x, (int)e->y, e->size, (Color){255,255,255,60});

        // Barre de vie
        float ratio = e->hp / e->max_hp;
        int   bw    = (int)(e->size * 2.5f);
        int   bx    = (int)e->x - bw / 2;
        int   by    = (int)e->y - (int)e->size - 6;
        DrawRectangle(bx, by, bw, 3, (Color){30,10,10,200});
        Color hp_col = ratio > 0.6f ? (Color){46,204,113,255}
                     : ratio > 0.3f ? (Color){243,156,18,255}
                                    : (Color){231,76,60,255};
        DrawRectangle(bx, by, (int)(bw*ratio), 3, hp_col);

        // Label type
        DrawText(ENEMY_BASE_STATS[e->type].name, bx, by-12, 8,
                 (Color){200,200,180,180});
    }
}

// ════════════════════════════════════════════════════
// TOURS ET PROJECTILES
// ════════════════════════════════════════════════════
void render_towers(const TowerPool *tp) {
    for (int i = 0; i < MAX_TOWERS; i++) {
        const Tower *tw = &tp->towers[i];
        if (!tw->active) continue;

        int px = tw->tile_x * TILE_SIZE;
        int py = tw->tile_y * TILE_SIZE;
        int s  = TILE_SIZE;
        Color col = TOWER_FILL[tw->type];

        DrawRectangle(px+2, py+2, s-4, s-4, (Color){30,20,10,255});
        DrawRectangle(px+6, py+6, s-12, s-12, col);

        float cx = tw->cx, cy = tw->cy;
        float ex = cx + cosf(tw->angle) * (s*0.4f);
        float ey = cy + sinf(tw->angle) * (s*0.4f);
        DrawLineEx((Vector2){cx,cy}, (Vector2){ex,ey}, 3.0f, col);
        DrawRectangleLines(px+6, py+6, s-12, s-12, (Color){255,255,255,60});

        for (int l = 0; l < tw->level && l < 3; l++)
            DrawRectangle(px+4+l*4, py+s-6, 3, 3, (Color){255,215,0,255});
    }
}

void render_projectiles(const TowerPool *tp) {
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        const Projectile *p = &tp->projectiles[i];
        if (!p->active) continue;

        Color col = PROJ_COLOR[p->origin];
        if (p->splash) {
            DrawCircle((int)p->x, (int)p->y, 5, col);
            DrawCircleLines((int)p->x, (int)p->y, 8, (Color){col.r,col.g,col.b,100});
        } else {
            DrawCircle((int)p->x, (int)p->y, 3, col);
        }
    }
}

void render_tower_preview(const Map *map, const TowerPool *tp,
                           TowerType type, int tile_x, int tile_y)
{
    if (!tower_can_place(tp, map, tile_x, tile_y)) return;

    const TowerStats *st = &TOWER_BASE_STATS[type];
    Color col = TOWER_FILL[type];
    int px = tile_x * TILE_SIZE, py = tile_y * TILE_SIZE;

    DrawRectangle(px+6, py+6, TILE_SIZE-12, TILE_SIZE-12,
                  (Color){col.r,col.g,col.b,100});

    float range_px = st->range * TILE_SIZE;
    DrawCircleLines(px+TILE_SIZE/2, py+TILE_SIZE/2,
                    range_px, (Color){col.r,col.g,col.b,150});
    DrawCircle(px+TILE_SIZE/2, py+TILE_SIZE/2,
               range_px, (Color){col.r,col.g,col.b,20});
}

// ════════════════════════════════════════════════════
// UNITÉS
// ════════════════════════════════════════════════════
void render_units(const UnitPool *up) {
    DrawCircleLines((int)up->base_px, (int)up->base_py,
                    UNIT_BASE_STATS[UNIT_SOLDIER].intercept_range * TILE_SIZE,
                    (Color){231,76,60,40});

    for (int i = 0; i < MAX_UNITS; i++) {
        const Unit *u = &up->units[i];
        if (!u->active) continue;

        Color col = UNIT_FILL[u->type];

        if (u->target_idx != -1 && u->state != USTATE_PATROL) {
            DrawLineEx((Vector2){u->x, u->y},
                       (Vector2){u->x + cosf(u->patrol_angle)*20,
                                 u->y + sinf(u->patrol_angle)*20},
                       1.0f, (Color){col.r,col.g,col.b,80});
        }

        if (u->type == UNIT_MEDIC) {
            DrawCircle((int)u->x, (int)u->y, (int)(3.0f*TILE_SIZE),
                       (Color){231,76,60,20});
            DrawCircleLines((int)u->x, (int)u->y, (int)(3.0f*TILE_SIZE),
                            (Color){231,76,60,80});
        }

        DrawEllipse((int)u->x+1, (int)u->y+(int)u->size,
                    (int)(u->size*0.8f), (int)(u->size*0.3f),
                    (Color){0,0,0,60});
        DrawCircle((int)u->x, (int)u->y, (int)u->size, col);
        DrawCircleLines((int)u->x, (int)u->y, (int)u->size,
                        (Color){255,255,255,80});

        const char *icon =
            u->state==USTATE_ATTACK ? "X" :
            u->state==USTATE_CHASE  ? ">" :
            u->state==USTATE_RETURN ? "<" :
            u->state==USTATE_HEAL   ? "+" : ".";
        DrawText(icon, (int)u->x-3, (int)u->y-4, 8, (Color){255,255,255,200});

        int bw = (int)(u->size*2.5f);
        int bx = (int)u->x - bw/2;
        int by = (int)u->y - (int)u->size - 6;
        DrawRectangle(bx, by, bw, 3, (Color){10,30,10,200});
        float ratio = u->hp / u->max_hp;
        DrawRectangle(bx, by, (int)(bw*ratio), 3,
                      ratio > 0.5f ? (Color){46,204,113,255}
                                   : (Color){231,76,60,255});
    }
}

// ════════════════════════════════════════════════════
// GAME OVER — coordonnées virtuelles (RenderTexture 1120×770)
// ════════════════════════════════════════════════════
void render_gameover(const GameState *gs) {
    const int VIRT_W = MAP_W * TILE_SIZE;
    const int VIRT_H = MAP_H * TILE_SIZE + UI_HUD_HEIGHT;
    int cx = VIRT_W / 2, cy = VIRT_H / 2;

    DrawRectangle(0, 0, VIRT_W, VIRT_H, (Color){0, 0, 0, 160});

    int pw = 340, ph = 160;
    int px = cx - pw/2, py = cy - ph/2;
    DrawRectangle(px, py, pw, ph, (Color){12, 4, 4, 245});
    DrawRectangleLinesEx(
        (Rectangle){(float)px,(float)py,(float)pw,(float)ph},
        2.0f, (Color){231, 76, 60, 255});

    int tw = MeasureText("BASTION TOMBE", 26);
    DrawText("BASTION TOMBE",
             cx - tw/2, py + 16, 26, (Color){231, 76, 60, 255});
    DrawLine(px + 20, py + 50, px + pw - 20, py + 50,
             (Color){80, 20, 20, 180});

    DrawText(TextFormat("Vague atteinte : %d", gs->wave_manager.number),
             cx - 92, py + 62, 16, (Color){180, 160, 130, 255});
    DrawText("Ferraille gagnee — voir menu",
             cx - 108, py + 84, 14, (Color){127, 200, 127, 255});
    tw = MeasureText("[ESPACE] retour au menu", 13);
    DrawText("[ESPACE] retour au menu",
             cx - tw/2, py + ph - 24, 13, (Color){100, 80, 50, 255});
}