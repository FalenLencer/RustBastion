#include "raylib.h"
#include "raymath.h"
#include "game_state.h"
#include "map_gen.h"
#include "pathfinding.h"
#include "renderer.h"
#include "theme.h"
#include "tower.h"
#include "unit.h"
#include "enemy.h"
#include "meta.h"
#include "ui.h"
#include "menu.h"
#include "save.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

// ════════════════════════════════════════════════════
// CANVAS VIRTUEL — 1120 × 770 px
// ════════════════════════════════════════════════════
#define VIRT_W  (MAP_W * TILE_SIZE)
#define VIRT_H  (MAP_H * TILE_SIZE + UI_HUD_HEIGHT)

typedef enum { SCREEN_MENU = 0, SCREEN_GAME } Screen;

typedef enum {
    INTER_NONE      = 0,
    INTER_STAGE_WIN,    // stage de campagne remporté
} InterludeState;

// ════════════════════════════════════════════════════
// HELPERS
// ════════════════════════════════════════════════════
static void apply_window_size(int w, int h) {
    SetWindowSize(w, h);
    int mw = GetMonitorWidth(0), mh = GetMonitorHeight(0);
    Vector2 mp = GetMonitorPosition(0);
    SetWindowPosition((int)(mp.x+(mw-w)/2), (int)(mp.y+(mh-h)/2));
}

static void compute_canvas(int sw, int sh,
                            float *s, float *ox, float *oy) {
    float sx = (float)sw / VIRT_W, sy = (float)sh / VIRT_H;
    *s = sx < sy ? sx : sy;
    if (*s < 0.25f) *s = 0.25f;
    if (*s > 4.00f) *s = 4.00f;
    *ox = floorf((sw - VIRT_W * *s) * 0.5f);
    *oy = floorf((sh - VIRT_H * *s) * 0.5f);
}

static void new_map(GameState *gs, ThemeID theme) {
    int seed;
    do {
        seed = GetRandomValue(1, 99999);
        generate_map(&gs->map, seed, 20, theme);
        astar_all(&gs->map, &gs->enemy_paths);
        pathset_apply(&gs->map, &gs->enemy_paths);
    } while (gs->enemy_paths.count == 0 || gs->map.path_count == 0);
    float bpx = gs->map.paths[0].base.x * TILE_SIZE + TILE_SIZE/2.0f;
    float bpy = gs->map.paths[0].base.y * TILE_SIZE + TILE_SIZE/2.0f;
    unit_pool_init(&gs->units, bpx, bpy);
    ui_init(&gs->ui);
    printf("Map seed=%d chemins=%d theme=%d\n",
           seed, gs->enemy_paths.count, (int)gs->map.theme);
}

static void start_arcade(GameState *gs, ThemeID theme, int slot) {
    game_state_init(gs);
    gs->is_campaign = 0;
    gs->campaign_num = 0;
    gs->campaign_stage = 0;
    new_map(gs, theme);
    printf("Arcade slot=%d theme=%d\n", slot, (int)theme);
}

static void start_campaign(GameState *gs, int campaign_num, int slot) {
    game_state_init(gs);
    gs->is_campaign    = 1;
    gs->campaign_num   = campaign_num;
    gs->campaign_stage = 0;
    int themes[CAMPAIGN_STAGES];
    meta_campaign_theme_order(campaign_num, themes);
    new_map(gs, (ThemeID)themes[0]);
    printf("Campagne %d stage 0 slot=%d theme=%d\n",
           campaign_num, slot, themes[0]);
}

static void next_campaign_stage(GameState *gs) {
    int camp_num   = gs->campaign_num;
    int camp_stage = gs->campaign_stage + 1;
    MetaProgress meta_bak = gs->meta;
    game_state_init(gs);
    gs->is_campaign    = 1;
    gs->campaign_num   = camp_num;
    gs->campaign_stage = camp_stage;
    gs->meta           = meta_bak;
    meta_compute(&gs->meta, &gs->bonuses);
    gs->gold  = gs->bonuses.start_gold;
    gs->lives = gs->bonuses.start_lives;
    int themes[CAMPAIGN_STAGES];
    meta_campaign_theme_order(camp_num, themes);
    new_map(gs, (ThemeID)themes[camp_stage]);
    printf("Campagne %d stage %d theme=%d\n", camp_num, camp_stage, themes[camp_stage]);
}

// ════════════════════════════════════════════════════
// INTERLUDE ENTRE STAGES
// ════════════════════════════════════════════════════
static void render_interlude(const GameState *gs,
                              int scrap_earned, int last_stage)
{
    const int CX = VIRT_W/2, CY = VIRT_H/2;
    DrawRectangle(0, 0, VIRT_W, VIRT_H, (Color){0,0,0,200});

    int pw = 460, ph = last_stage ? 260 : 220;
    DrawRectangle(CX-pw/2, CY-ph/2, pw, ph, (Color){10,6,2,250});
    DrawRectangleLinesEx(
        (Rectangle){(float)(CX-pw/2),(float)(CY-ph/2),(float)pw,(float)ph},
        2.5f, (Color){46,204,113,255});

    const char *title = last_stage ? "CAMPAGNE TERMINEE !" : "STAGE TERMINE !";
    int tw = MeasureText(title, 22);
    DrawText(title, CX-tw/2, CY-ph/2+18, 22,
             last_stage ? (Color){239,159,39,255} : (Color){46,204,113,255});
    DrawLine(CX-pw/2+20, CY-ph/2+48, CX+pw/2-20, CY-ph/2+48,
             (Color){50,35,15,200});

    DrawText(TextFormat("Vague atteinte  : %d", gs->wave_manager.number),
             CX-pw/2+24, CY-ph/2+60, 14, (Color){180,160,130,255});
    DrawText(TextFormat("Ferraille gagnee : +%d", scrap_earned),
             CX-pw/2+24, CY-ph/2+82, 14, (Color){127,200,50,255});
    DrawText(TextFormat("Ferraille totale : %d", gs->meta.scrap),
             CX-pw/2+24, CY-ph/2+104, 12, (Color){80,120,60,255});

    if (!last_stage) {
        int themes[CAMPAIGN_STAGES];
        meta_campaign_theme_order(gs->campaign_num, themes);
        const Theme *nth = theme_get((ThemeID)themes[gs->campaign_stage+1]);
        DrawText(TextFormat("Prochain : %s", nth->name),
                 CX-pw/2+24, CY-ph/2+130, 13, (Color){100,160,200,255});
    } else {
        DrawText(TextFormat("Campagnes terminees : %d",
                     gs->meta.campaigns_completed),
                 CX-pw/2+24, CY-ph/2+130, 13, (Color){239,159,39,255});
    }

    const char *hint = last_stage ? "[ESPACE] Retour au menu"
                                  : "[ESPACE] Continuer";
    tw = MeasureText(hint, 14);
    DrawText(hint, CX-tw/2, CY+ph/2-36, 14, (Color){160,140,100,255});
}

// ════════════════════════════════════════════════════
// MAIN
// ════════════════════════════════════════════════════
int main(void) {
    InitWindow(VIRT_W, VIRT_H, "RUST BASTION");
    
    Image icon = LoadImage("assets/icon.png");
    SetWindowIcon(icon);
    UnloadImage(icon);

    SetTargetFPS(60);
    SetExitKey(KEY_NULL);
    {
        int mw = GetMonitorWidth(0), mh = GetMonitorHeight(0);
        Vector2 mp = GetMonitorPosition(0);
        SetWindowPosition((int)(mp.x+(mw-VIRT_W)/2),
                          (int)(mp.y+(mh-VIRT_H)/2));
    }

    RenderTexture2D canvas = LoadRenderTexture(VIRT_W, VIRT_H);
    SetTextureFilter(canvas.texture, TEXTURE_FILTER_BILINEAR);
    RENDER_SCALE = 1.0f;

    GameState gs           = {0};
    Screen    screen       = SCREEN_MENU;
    int       active_slot  = -1;
    InterludeState interlude     = INTER_NONE;
    int            interlude_scrap = 0;
    int            interlude_last  = 0;

    game_state_init(&gs);

    AppOptions opts = { .win_width=VIRT_W, .win_height=VIRT_H };
    MenuState menu  = {0};
    menu_init(&menu, &opts);

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        int sw = GetScreenWidth(), sh = GetScreenHeight();
        float csc, cox, coy;
        compute_canvas(sw, sh, &csc, &cox, &coy);
        ui_set_mouse_offset(cox, coy, csc);
        menu_set_mouse_offset(cox, coy, csc);

        BeginTextureMode(canvas);

        // ════════════════════════════════════════════════════
        if (screen == SCREEN_MENU) {
        // ════════════════════════════════════════════════════

            menu_update(&menu, &gs.meta);
            ClearBackground((Color){8,5,3,255});
            MenuAction act = menu_render_and_act(&menu, &gs.meta, VIRT_W, VIRT_H);

            if (act.quit_app) { EndTextureMode(); break; }
            if (act.toggle_fs == 1) ToggleFullscreen();
            if (act.toggle_fs == 2 && !IsWindowFullscreen())
                apply_window_size(menu.opts.win_width, menu.opts.win_height);

            if (act.start_arcade) {
                active_slot = act.new_slot;
                start_arcade(&gs, act.new_theme, active_slot);
                save_write(&gs, active_slot);
                menu.screen = MENU_TITLE;
                screen = SCREEN_GAME;
            }

            if (act.start_campaign) {
                active_slot = act.new_slot;
                start_campaign(&gs, gs.meta.campaigns_completed, active_slot);
                save_write(&gs, active_slot);
                menu.screen = MENU_TITLE;
                screen = SCREEN_GAME;
            }

            if (act.go_game && !act.start_arcade && !act.start_campaign) {
                int slot = act.resume_slot;
                if (save_read(&gs, slot)) {
                    active_slot = slot;
                    menu.screen = MENU_TITLE;
                    screen = SCREEN_GAME;
                    printf("Reprise slot %d\n", slot);
                } else {
                    active_slot = slot;
                    start_arcade(&gs, THEME_COUNT, slot);
                    save_write(&gs, active_slot);
                    screen = SCREEN_GAME;
                }
            }

        // ════════════════════════════════════════════════════
        } else {   // SCREEN_GAME
        // ════════════════════════════════════════════════════

            if (IsKeyPressed(KEY_ESCAPE)) {
                menu.paused ^= 1;
                menu.screen = menu.paused ? MENU_PAUSE : MENU_TITLE;
            }

            // Mise à jour jeu (hors pause et interlude)
            if (!menu.paused && interlude == INTER_NONE) {
                ui_update(&gs.ui, &gs);
                game_state_update(&gs, dt);
            }

            // ── Game over ─────────────────────────────────────
            if (gs.phase == PHASE_GAMEOVER && interlude == INTER_NONE) {
                if (IsKeyPressed(KEY_SPACE)) {
                    if (active_slot >= 0) save_delete(active_slot);
                    active_slot = -1;
                    menu_refresh_slots(&menu);
                    menu.paused = 0;
                    menu.screen = gs.is_campaign ? MENU_CAMPAIGN : MENU_ARCADE;
                    screen = SCREEN_MENU;
                }
            }

            // ── Fin de stage campagne (TAB en phase PREP) ─────
            if (gs.is_campaign &&
                gs.phase == PHASE_PREP &&
                gs.wave_manager.number >= 1 &&
                interlude == INTER_NONE &&
                IsKeyPressed(KEY_TAB))
            {
                int last = (gs.campaign_stage == CAMPAIGN_STAGES - 1);
                int earned = meta_end_of_campaign_stage(
                    &gs.meta,
                    gs.wave_manager.number,
                    gs.kills,
                    gs.gold,
                    gs.campaign_stage);
                interlude       = INTER_STAGE_WIN;
                interlude_scrap = earned;
                interlude_last  = last;
                save_write(&gs, active_slot);
            }

            // ── Interlude ─────────────────────────────────────
            if (interlude == INTER_STAGE_WIN && IsKeyPressed(KEY_SPACE)) {
                if (interlude_last) {
                    if (active_slot >= 0) save_delete(active_slot);
                    active_slot = -1;
                    menu_refresh_slots(&menu);
                    menu.paused = 0;
                    menu.screen = MENU_TITLE;
                    screen    = SCREEN_MENU;
                    interlude = INTER_NONE;
                } else {
                    next_campaign_stage(&gs);
                    save_write(&gs, active_slot);
                    interlude = INTER_NONE;
                }
            }

            // ── Rendu ─────────────────────────────────────────
            ClearBackground(theme_get(gs.map.theme)->palette.bg);
            render_map(&gs.map);
            render_paths(&gs.enemy_paths);
            render_towers(&gs.towers);
            render_units(&gs.units);
            render_enemies(&gs.enemies);
            render_projectiles(&gs.towers);
            ui_render(&gs.ui, &gs);

            // Badge campagne
            if (gs.is_campaign) {
                int themes[CAMPAIGN_STAGES];
                meta_campaign_theme_order(gs.campaign_num, themes);
                const Theme *sth = theme_get((ThemeID)themes[gs.campaign_stage]);
                DrawText(TextFormat("CAMPAGNE %d  |  Stage %d/%d  |  %s",
                             gs.campaign_num+1, gs.campaign_stage+1,
                             CAMPAIGN_STAGES, sth->name),
                         8, 8, 11, (Color){200,180,120,210});
                if (gs.phase == PHASE_PREP && gs.wave_manager.number >= 1)
                    DrawText("[TAB] Terminer ce stage et progresser",
                             8, 22, 10, (Color){100,180,80,200});
            }

            if (gs.phase == PHASE_GAMEOVER)
                render_gameover(&gs);

            if (interlude == INTER_STAGE_WIN)
                render_interlude(&gs, interlude_scrap, interlude_last);

            // ── Pause overlay ─────────────────────────────────
            if (menu.paused) {
                MenuAction pact = menu_render_and_act(
                    &menu, &gs.meta, VIRT_W, VIRT_H);
                if (pact.save_and_quit == 2 && active_slot >= 0) {
                    save_write(&gs, active_slot);
                    menu_refresh_slots(&menu);
                }
                if (pact.save_and_quit == 1) {
                    if (active_slot >= 0) save_write(&gs, active_slot);
                    menu_refresh_slots(&menu);
                    menu.paused = 0;
                    menu.screen = gs.is_campaign ? MENU_CAMPAIGN : MENU_ARCADE;
                    screen = SCREEN_MENU;
                    interlude = INTER_NONE;
                }
                if (pact.quit_app) {
                    if (active_slot >= 0) save_write(&gs, active_slot);
                    EndTextureMode(); break;
                }
                if (pact.toggle_fs == 1) ToggleFullscreen();
                if (pact.toggle_fs == 2 && !IsWindowFullscreen())
                    apply_window_size(menu.opts.win_width, menu.opts.win_height);
            }
        }

        EndTextureMode();

        BeginDrawing();
        ClearBackground(BLACK);
        DrawTexturePro(canvas.texture,
            (Rectangle){0,0,(float)canvas.texture.width,
                             -(float)canvas.texture.height},
            (Rectangle){cox, coy, VIRT_W*csc, VIRT_H*csc},
            (Vector2){0,0}, 0.0f, WHITE);
        EndDrawing();
    }

    if (screen == SCREEN_GAME && active_slot >= 0 &&
        gs.phase != PHASE_GAMEOVER && interlude == INTER_NONE)
        save_write(&gs, active_slot);

    UnloadRenderTexture(canvas);
    CloseWindow();
    return 0;
}