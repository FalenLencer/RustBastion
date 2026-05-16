#include "raylib.h"
#include "raymath.h"
#include "game/game_state.h"
#include "engine/window.h"
#include "engine/canvas.h"
#include "game/game_init.h"
#include "game/save.h"
#include "ui/renderer.h"
#include "ui/hud.h"
#include "ui/menu.h"
#include "ui/interlude.h"
#include "map/theme.h"
#include "engine/audio.h"
#include "combat/tower.h"
#include "combat/unit.h"
#include "combat/enemy.h"
#include "game/meta.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "engine/paths.h"
#ifdef _WIN32
#  include <windows.h>
#  include <direct.h>
#else
#  include <sys/stat.h>
#  include <unistd.h>
#endif

char g_data_prefix[256] = "";

typedef enum { SCREEN_MENU = 0, SCREEN_GAME } Screen;
typedef enum {
    INTER_NONE         = 0,
    INTER_DIALOG_BEFORE,   // ← dialogue avant l'acte
    INTER_STAGE_WIN,
    INTER_DIALOG_AFTER,    // ← dialogue après l'acte
    INTER_EXTRACT,
} InterludeState;

// Dessine la fenêtre EXTRAIRE / CONTINUER en endless
// vmouse : souris déjà convertie en coordonnées virtuelles
static void draw_extract_screen(const GameState *gs, int vw, int vh,
                                Vector2 vmouse) {
    int cx = vw/2, cy = vh/2;
    DrawRectangle(0, 0, vw, vh, (Color){0,0,0,170});

    int pw = 420, ph = 260;
    float rnd = 5.0f/ph;
    DrawRectangleRounded(
        (Rectangle){cx-pw/2.0f, cy-ph/2.0f, (float)pw, (float)ph},
        rnd, 8, (Color){10,6,2,252});
    DrawRectangleRoundedLinesEx(
        (Rectangle){cx-pw/2.0f, cy-ph/2.0f, (float)pw, (float)ph},
        rnd, 8, 2.0f, (Color){232,152,32,255});

    int px = cx - pw/2 + 16;
    int py = cy - ph/2 + 16;
    int iw = pw - 32;

    // Titre
    const char *t1 = "POINT D'EXTRACTION";
    DrawText(t1, cx - MeasureText(t1,18)/2, py, 18, (Color){232,152,32,255});
    py += 24;
    DrawLine(px, py, px+iw, py, (Color){60,40,12,180}); py += 10;

    // Infos
    DrawText(TextFormat("Serie       : %d", gs->endless_series+1),
             px, py, 12, (Color){168,148,102,255}); py += 16;
    DrawText(TextFormat("Vague       : %d", gs->wave_manager.number),
             px, py, 12, (Color){168,148,102,255}); py += 16;
    DrawText(TextFormat("Multiplicateur : x%.1f", gs->endless_multiplier),
             px, py, 12, (Color){232,152,32,255}); py += 16;

    int score = meta_endless_score(gs->wave_manager.number, gs->endless_multiplier);
    int scrap  = score / 10 > 200 ? 200 : score / 10;
    DrawText(TextFormat("Ferraille si extrait : +%d", scrap),
             px, py, 12, (Color){118,188,45,255}); py += 16;

    float next_mult = gs->endless_multiplier * 1.5f;
    DrawText(TextFormat("Continuer → multiplicateur x%.1f", next_mult),
             px, py, 10, (Color){100,160,220,255}); py += 20;

    DrawLine(px, py, px+iw, py, (Color){40,28,8,140}); py += 10;

    // Boutons
    int bw = 160, bh = 32;
    int by2 = cy + ph/2 - 16 - bh;
    int bx1 = cx - bw - 8;
    int bx2 = cx + 8;

    // EXTRAIRE
    {
        Rectangle r = {(float)bx1,(float)by2,(float)bw,(float)bh};
        int hov = CheckCollisionPointRec(vmouse, r);
        DrawRectangleRounded(r, 5.0f/bh, 6,
            hov ? (Color){8,28,8,255} : (Color){4,16,4,255});
        DrawRectangleRoundedLinesEx(r, 5.0f/bh, 6, 1.5f,
            hov ? (Color){42,190,105,255} : (Color){20,80,40,255});
        const char *lbl = "[E] EXTRAIRE";
        DrawText(lbl, bx1+bw/2-MeasureText(lbl,13)/2, by2+bh/2-7, 13,
                 (Color){42,190,105,255});
    }
    // CONTINUER
    {
        Rectangle r = {(float)bx2,(float)by2,(float)bw,(float)bh};
        int hov = CheckCollisionPointRec(vmouse, r);
        DrawRectangleRounded(r, 5.0f/bh, 6,
            hov ? (Color){6,18,32,255} : (Color){4,12,20,255});
        DrawRectangleRoundedLinesEx(r, 5.0f/bh, 6, 1.5f,
            hov ? (Color){52,140,210,255} : (Color){24,70,110,255});
        const char *lbl = "[ESPACE] CONTINUER";
        DrawText(lbl, bx2+bw/2-MeasureText(lbl,11)/2, by2+bh/2-7, 11,
                 (Color){52,140,210,255});
    }
}

// ── Répertoire de base ────────────────────────────────────────────
// Se place dans le répertoire qui contient assets/ :
//   - Build de dev   : exe est dans build/, assets/ est dans le parent → chdir(parent)
//   - Package Linux  : assets/ est à côté de l'exe                     → chdir(exe_dir)
//   - Package Windows: idem
// Tous les chemins relatifs (saves/, config/, rustbastion.sav) sont
// ainsi toujours résolus au bon endroit, quelle que soit la cwd de lancement.
static void setup_working_dir(void) {
    char exe[512] = {0};
    char dir[512] = {0};

#ifdef _WIN32
    if (!GetModuleFileNameA(NULL, exe, sizeof(exe))) return;
    // Isole le répertoire de l'exe
    char *sep = strrchr(exe, '\\');
    if (!sep) return;
    *sep = '\0';
    strncpy(dir, exe, sizeof(dir) - 1);

    // Cherche assets\ dans le répertoire de l'exe, puis dans le parent
    char test[512];
    snprintf(test, sizeof(test), "%s\\assets", dir);
    DWORD attr = GetFileAttributesA(test);
    if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) {
        _chdir(dir); return;
    }
    sep = strrchr(dir, '\\');
    if (sep) {
        *sep = '\0';
        snprintf(test, sizeof(test), "%s\\assets", dir);
        attr = GetFileAttributesA(test);
        if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) {
            _chdir(dir);
            snprintf(g_data_prefix, sizeof(g_data_prefix), "build/");
        }
    }
#else
    ssize_t n = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
    if (n <= 0) return;
    exe[n] = '\0';

    // Isole le répertoire de l'exe
    char *sep = strrchr(exe, '/');
    if (!sep) return;
    *sep = '\0';
    snprintf(dir, sizeof(dir), "%s", exe);

    // Cherche assets/ dans le répertoire de l'exe, puis dans le parent
    char test[512];
    struct stat st;
    snprintf(test, sizeof(test), "%s/assets", dir);
    if (stat(test, &st) == 0 && S_ISDIR(st.st_mode)) {
        int r = chdir(dir); (void)r; return;
    }
    sep = strrchr(dir, '/');
    if (sep) {
        *sep = '\0';
        snprintf(test, sizeof(test), "%s/assets", dir);
        if (stat(test, &st) == 0 && S_ISDIR(st.st_mode)) {
            int r = chdir(dir); (void)r;
            snprintf(g_data_prefix, sizeof(g_data_prefix), "build/");
        }
    }
#endif
}

// ── Persistance des options ───────────────────────────────────────
#define OPTS_MAGIC     0x52424F50u   // "RBOP"
#define OPTS_VERSION   1

typedef struct { unsigned int magic; int version; AppOptions opts; } OptsFile;

static void opts_save(const AppOptions *o) {
    data_mkdir("config");
    char path[512];
    FILE *f = fopen(data_path(path, sizeof(path), "config/settings.bin"), "wb");
    if (!f) return;
    OptsFile hdr = { OPTS_MAGIC, OPTS_VERSION, *o };
    fwrite(&hdr, sizeof(hdr), 1, f);
    fclose(f);
}

static int opts_load(AppOptions *o) {
    char path[512];
    FILE *f = fopen(data_path(path, sizeof(path), "config/settings.bin"), "rb");
    if (!f) return 0;
    OptsFile hdr;
    int ok = (fread(&hdr, sizeof(hdr), 1, f) == 1)
          && hdr.magic   == OPTS_MAGIC
          && hdr.version == OPTS_VERSION;
    fclose(f);
    if (ok) *o = hdr.opts;
    return ok;
}

int main(void) {
    setup_working_dir();   // doit être appelé avant tout accès fichier
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(VIRT_W, VIRT_H, "RUST BASTION");
    window_disable_vsync();
    SetTargetFPS(60);
    SetExitKey(KEY_NULL);
    window_center();

    audio_init();
    save_init();

    Image icon = LoadImage("assets/icon.png");
    SetWindowIcon(icon);
    UnloadImage(icon);

    RenderTexture2D canvas = LoadRenderTexture(VIRT_W, VIRT_H);
    SetTextureFilter(canvas.texture, TEXTURE_FILTER_BILINEAR);
    RENDER_SCALE = 1.0f;

    static GameState gs;
    memset(&gs, 0, sizeof(gs));
    Screen         screen          = SCREEN_MENU;
    int            active_slot     = -1;
    InterludeState interlude       = INTER_NONE;
    int            interlude_scrap = 0;
    int            interlude_last  = 0;
    int            interlude_stars = 0;

    game_state_init(&gs);

    AppOptions opts = {
        .win_width     = VIRT_W,
        .win_height    = VIRT_H,
        .target_fps    = 60,
        .master_volume = 50,
        .music_volume  = 50,
        .sfx_volume    = 50,
    };
    opts_load(&opts);   // écrase les valeurs par défaut si un fichier existe
    // Applique les volumes au module audio AVANT que menu_init ne les relise
    audio_set_master_volume(opts.master_volume / 100.0f);
    audio_set_music_volume (opts.music_volume  / 100.0f);
    audio_set_sfx_volume   (opts.sfx_volume    / 100.0f);

    MenuState menu  = {0};
    menu_init(&menu, &opts);
    int applied_fps = 60;
    MenuScreen prev_menu_screen = menu.screen;

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        audio_update();

        {
            int wfps = menu.opts.target_fps ? menu.opts.target_fps : 0;
            if (wfps != applied_fps) { SetTargetFPS(wfps); applied_fps = wfps; }
        }

        int sw = GetScreenWidth(), sh = GetScreenHeight();
        float csc, cox, coy;
        canvas_compute(sw, sh, &csc, &cox, &coy);
        canvas_set_mouse_offset(cox, coy, csc);
        menu_set_mouse_offset(cox, coy, csc);

        BeginTextureMode(canvas);

        // Sauvegarde des options quand on quitte l'écran Options
        if (prev_menu_screen == MENU_OPTIONS && menu.screen != MENU_OPTIONS)
            opts_save(&menu.opts);
        prev_menu_screen = menu.screen;

        if (screen == SCREEN_MENU) {
            menu_update(&menu, &gs.meta);
            ClearBackground((Color){8,5,3,255});
            MenuAction act = menu_render_and_act(&menu, &gs.meta, VIRT_W, VIRT_H);

            if (act.quit_app)       { EndTextureMode(); break; }
            if (act.toggle_fs == 1) ToggleFullscreen();
            if (act.toggle_fs == 2 && !IsWindowFullscreen())
                window_apply_size(menu.opts.win_width, menu.opts.win_height);

            if (act.start_arcade) {
                active_slot = act.new_slot;
                game_init_arcade(&gs, act.new_theme, active_slot);
                save_write(&gs, active_slot);
                menu.screen = MENU_TITLE;
                screen = SCREEN_GAME;
                audio_play_theme_music(gs.map.theme);
            }

            if (act.start_campaign) {
                active_slot = act.new_slot;
                gs.campaign_order_seed = act.campaign_order_seed;
                game_init_campaign(&gs, gs.meta.campaigns_completed,
                                   active_slot, act.campaign_order_seed);
                save_write(&gs, active_slot);
                menu.screen = MENU_TITLE;
                screen = SCREEN_GAME;
                audio_play_theme_music(gs.map.theme);
                interlude = INTER_DIALOG_BEFORE;
            }

            if (act.go_game && !act.start_arcade && !act.start_campaign) {
                int slot = act.resume_slot;
                if (save_read(&gs, slot)) {
                    active_slot = slot;
                    menu.screen = MENU_TITLE;
                    screen = SCREEN_GAME;
                } else {
                    active_slot = slot;
                    game_init_arcade(&gs, THEME_COUNT, slot);
                    save_write(&gs, active_slot);
                    screen = SCREEN_GAME;
                }
                audio_play_theme_music(gs.map.theme);
            }

        } else { // SCREEN_GAME

            if (IsKeyPressed(KEY_ESCAPE)) {
                menu.paused ^= 1;
                menu.screen = menu.paused ? MENU_PAUSE : MENU_TITLE;
            }

            if (!menu.paused && interlude == INTER_NONE) {
                ui_update(&gs.ui, &gs);
                game_state_update(&gs, dt);

                // ── Déclenchement extraction endless ──────────────
                if (gs.is_endless && !gs.endless_pending_extract &&
                    gs.phase == PHASE_PREP &&
                    gs.wave_manager.number > 0 &&
                    gs.wave_manager.number % 10 == 0)
                {
                    gs.endless_pending_extract = 1;
                    interlude = INTER_EXTRACT;
                }
            }

            // ── Game over ─────────────────────────────────────────
            if (gs.phase == PHASE_GAMEOVER && interlude == INTER_NONE) {
                // En endless : 0 ferraille si pas extrait
                if (gs.is_endless) {
                    meta_endless_end(&gs.meta,
                                     gs.wave_manager.number,
                                     gs.endless_multiplier, 0);
                    menu_refresh_slots(&menu);
                }
                if (IsKeyPressed(KEY_SPACE)) {
                    if (active_slot >= 0) save_delete(active_slot);
                    active_slot = -1;
                    menu_refresh_slots(&menu);
                    menu.paused = 0;
                    menu.screen = gs.is_campaign ? MENU_WORLD_MAP : MENU_ARCADE;
                    screen = SCREEN_MENU;
                    audio_stop_music();
                }
            }

            // ── Fin de stage campagne ─────────────────────────────
            if (gs.is_campaign && gs.phase == PHASE_PREP &&
                gs.wave_manager.number >= 1 &&
                interlude == INTER_NONE && IsKeyPressed(KEY_TAB))
            {
                int stage_idx = gs.campaign_stage;
                const ActData *ad = campaign_act_get(stage_idx);
                int last  = (stage_idx == CAMPAIGN_TOTAL - 1);
                int earned = meta_end_of_campaign_stage(
                    &gs.meta, gs.wave_manager.number,
                    gs.kills, gs.gold, stage_idx);
                int obj_ok = campaign_objective_check(
                    ad, gs.wave_manager.number, gs.kills,
                    gs.units.count,
                    gs.act_materials_collected,
                    gs.act_no_unit_lost);
                int stars = meta_record_act(&gs.meta, stage_idx, obj_ok, 0);
                interlude       = INTER_DIALOG_AFTER;
                interlude_scrap = earned;
                interlude_last  = last;
                interlude_stars = stars;
                save_write(&gs, active_slot);
            }

            // ── Interlude campagne — dialogue après l'acte ────────
            if (interlude == INTER_DIALOG_AFTER && IsKeyPressed(KEY_SPACE)) {
                if (interlude_last) {
                    if (active_slot >= 0) save_delete(active_slot);
                    active_slot = -1;
                    menu_refresh_slots(&menu);
                    menu.paused = 0;
                    menu.screen = MENU_WORLD_MAP;
                    screen = SCREEN_MENU;
                    interlude = INTER_NONE;
                    audio_stop_music();
                } else {
                    game_next_campaign_stage(&gs);
                    save_write(&gs, active_slot);
                    interlude = INTER_DIALOG_BEFORE;
                }
            }

            // ── Interlude extraction endless ──────────────────────
            if (interlude == INTER_EXTRACT) {
                // EXTRAIRE — [E]
                if (IsKeyPressed(KEY_E)) {
                    meta_endless_end(&gs.meta,
                                     gs.wave_manager.number,
                                     gs.endless_multiplier, 1);
                    menu_refresh_slots(&menu);
                    if (active_slot >= 0) save_delete(active_slot);
                    active_slot = -1;
                    menu.paused = 0;
                    menu.screen = MENU_ARCADE;
                    screen = SCREEN_MENU;
                    interlude = INTER_NONE;
                    audio_stop_music();
                }
                // CONTINUER — [ESPACE]
                else if (IsKeyPressed(KEY_SPACE)) {
                    // Monte le multiplicateur : ×1 → ×1.5 → ×2 → ×3
                    gs.endless_multiplier *= 1.5f;
                    if (gs.endless_multiplier > 6.0f) gs.endless_multiplier = 6.0f;
                    gs.endless_series++;
                    gs.endless_pending_extract = 0;

                    // Change de thème toutes les 10 vagues
                    ThemeID next_theme = (ThemeID)((gs.map.theme + 1) % THEME_COUNT);
                    game_init_map(&gs, next_theme);
                    audio_play_theme_music(gs.map.theme);
                    interlude = INTER_NONE;
                }
            }

            // ── Rendu ─────────────────────────────────────────────
            ClearBackground(theme_get(gs.map.theme)->palette.bg);
            render_map(&gs.map);
            render_spawn_exclusion_zones(&gs.map);
            render_bases(&gs.map);
            render_deposits(&gs.map);
            render_paths(&gs.enemy_paths);
            render_towers(&gs.towers);
            render_units(&gs.units);
            render_enemies(&gs.enemies);
            render_projectiles(&gs.towers);
            ui_render(&gs.ui, &gs);

            // Badge campagne
            if (gs.is_campaign && interlude == INTER_NONE) {
                const ActData *cam_ad = campaign_act_get(gs.campaign_stage);
                DrawText(TextFormat("CH.%d  |  %s  |  Acte %d/%d",
                             cam_ad->chapter+1, cam_ad->title,
                             gs.campaign_stage+1, CAMPAIGN_TOTAL),
                         8, 8, 11, (Color){200,180,120,210});
                if (gs.phase == PHASE_PREP && gs.wave_manager.number >= 1)
                    DrawText(TextFormat("[TAB] Terminer l'acte   Obj: %s",
                                 cam_ad->objective.description),
                             8, 22, 9, (Color){100,180,80,200});
            }

            // Badge endless
            if (gs.is_endless) {
                int score = (int)((float)gs.wave_manager.number
                                  * gs.endless_multiplier * 10.0f);
                DrawText(TextFormat("ENDLESS  |  Serie %d  |  x%.1f  |  Score %d",
                             gs.endless_series+1,
                             gs.endless_multiplier, score),
                         8, 8, 11, (Color){140,200,240,210});

                // Prochaine extraction dans N vagues
                int next_extr = 10 - (gs.wave_manager.number % 10);
                if (next_extr == 10 && gs.wave_manager.number > 0) next_extr = 0;
                if (next_extr > 0)
                    DrawText(TextFormat("Extraction dans %d vague(s)", next_extr),
                             8, 22, 9, (Color){80,140,180,180});
            }

            if (gs.phase == PHASE_GAMEOVER)
                interlude_render_gameover(&gs, VIRT_W, VIRT_H);

            if (interlude == INTER_DIALOG_BEFORE) {
                const ActData *dlg_ad = campaign_act_get(gs.campaign_stage);
                interlude_render_dialog_before(dlg_ad, VIRT_W, VIRT_H);
                if (IsKeyPressed(KEY_SPACE))
                    interlude = INTER_NONE;
            }
            if (interlude == INTER_DIALOG_AFTER) {
                const ActData *dlg_ad = campaign_act_get(gs.campaign_stage);
                interlude_render_dialog_after(dlg_ad, interlude_stars,
                                              interlude_scrap, VIRT_W, VIRT_H);
            }

            // Fenêtre extraction endless
            if (interlude == INTER_EXTRACT) {
                Vector2 vm = { (GetMousePosition().x - cox) / csc,
                               (GetMousePosition().y - coy) / csc };
                draw_extract_screen(&gs, VIRT_W, VIRT_H, vm);
            }

            if (menu.paused) {
                MenuAction pact = menu_render_and_act(&menu, &gs.meta, VIRT_W, VIRT_H);
                if (pact.save_and_quit == 2 && active_slot >= 0)
                    save_write(&gs, active_slot);
                if (pact.save_and_quit == 1) {
                    if (active_slot >= 0) save_write(&gs, active_slot);
                    menu_refresh_slots(&menu);
                    menu.paused = 0;
                    menu.screen = gs.is_campaign ? MENU_CAMPAIGN : MENU_ARCADE;
                    screen = SCREEN_MENU;
                    interlude = INTER_NONE;
                    audio_stop_music();
                }
                if (pact.quit_app) {
                    if (active_slot >= 0) save_write(&gs, active_slot);
                    EndTextureMode(); break;
                }
                if (pact.toggle_fs == 1) ToggleFullscreen();
                if (pact.toggle_fs == 2 && !IsWindowFullscreen())
                    window_apply_size(menu.opts.win_width, menu.opts.win_height);
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

    opts_save(&menu.opts);
    menu_cleanup(&menu);
    audio_shutdown();
    UnloadRenderTexture(canvas);
    CloseWindow();
    return 0;
}