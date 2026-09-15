/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */
/* ════════════════════════════════════════════════════════════════
   ui/render3d_enemies.c — rendu 3D des ennemis (voir .h).

   Jumeau de render3d_units.c mais indexé par EnemyType et orienté sur
   la DIRECTION DE DÉPLACEMENT (les ennemis n'ont pas de cible explicite,
   ils suivent le chemin). Anim : Walk s'ils avancent, sinon Idle.
   ════════════════════════════════════════════════════════════════ */
#include "render3d_enemies.h"
#include "render3d.h"          /* render3d_yaw_for_aim (caméra oblique) */
#include "render3d_skin.h"     /* helpers jumeaux units/enemies         */
#include "../combat/combat_math.h" /* angle_approach (source unique)    */
#include "rlgl.h"
#include <math.h>
#include <string.h>
#include <stddef.h>
#include <ctype.h>

/* ── Réglages (à régler en jeu : aucun affichage côté outil) ─────── */
#define E_RT_W        160
#define E_RT_H        200
#define E_YAW_OFF     0.0f
#define E_REST_PHI    0.0f          /* modèles regardent +Z (raylib) au repos */
#define E_ANIM_FPS    24.0f
#define E_MOVE_EPS2   0.30f         /* seuil² (px) « bouge »                   */
#define E_TURN_SPEED  12.0f         /* vitesse de rotation vers la direction   */

/* (Lumière : R3D_LIGHT_DIR, source unique dans render3d.h.) */

/* (Shaders vertex-color : skin_vs_vc/skin_fs_vc, render3d_skin.h.) */

/* ── Modèle par type d'ennemi (cadrage PROPRE à chaque type) ──────── */
typedef struct {
    int             have;
    Model           model;
    ModelAnimation *anims;
    int             anim_count;
    int             a_idle, a_walk, a_attack;
    Camera3D        cam;
    float           dst_scale, dst_yanchor;
    float           rest_phi, yaw_off;            /* orientation PAR TYPE */
    float           gain;                          /* normalisation luminosité */
} EnemyModel;

static int             g_loaded = 0;
static Shader          g_shader_vc;
static int             g_loc_gain = -1;            /* uniform "gain" du shader */
static EnemyModel      g_em[ENEMY_TYPE_COUNT];

static RenderTexture2D g_rt[MAX_ENEMIES];
static int             g_used[MAX_ENEMIES];
static int             g_rt_type[MAX_ENEMIES];
static float           g_anim_t [MAX_ENEMIES];
static int             g_anim_i [MAX_ENEMIES];
static float           g_prev_x [MAX_ENEMIES];
static float           g_prev_y [MAX_ENEMIES];
static float           g_facing [MAX_ENEMIES];
static int             g_first  [MAX_ENEMIES];

/* (Matcher d'anims : skin_anim_match, render3d_skin.h.) */

/* (Application du shader : skin_apply_vc, render3d_skin.h.) */

/* (Gain de luminance : skin_model_gain, render3d_skin.h.) */

static void load_enemy(int type, const char *path, Camera3D cam,
                       float dst_scale, float dst_yanchor,
                       float rest_phi, float yaw_off) {
    EnemyModel *em = &g_em[type];
    em->model = LoadModel(path);
    if (em->model.meshCount == 0) { em->have = 0; return; }
    skin_apply_vc(&em->model, g_shader_vc);
    em->gain = skin_model_gain(&em->model);
    em->anims = LoadModelAnimations(path, &em->anim_count);
    static const char *KW_IDLE[]   = {"idle","stand","rest"};
    static const char *KW_WALK[]   = {"walk","run","move","march","drive","roll"};
    static const char *KW_ATTACK[] = {"attack","fight","hit","strike",
                                      "shoot","punch","slam","bite","melee",
                                      "mine","work"};
    int ai = skin_anim_match(em->anims, em->anim_count, KW_IDLE,   (int)(sizeof(KW_IDLE)  / sizeof(KW_IDLE[0])));
    int aw = skin_anim_match(em->anims, em->anim_count, KW_WALK,   (int)(sizeof(KW_WALK)  / sizeof(KW_WALK[0])));
    int aa = skin_anim_match(em->anims, em->anim_count, KW_ATTACK, (int)(sizeof(KW_ATTACK)/ sizeof(KW_ATTACK[0])));
    em->a_idle   = (ai >= 0) ? ai : 0;
    em->a_walk   = (aw >= 0) ? aw : em->a_idle;
    em->a_attack = (aa >= 0) ? aa : em->a_idle;
    em->cam = cam; em->dst_scale = dst_scale; em->dst_yanchor = dst_yanchor;
    em->rest_phi = rest_phi; em->yaw_off = yaw_off;
    em->have = 1;
}

void render3d_enemies_init(void) {
    g_loaded = 0;
    for (int i = 0; i < ENEMY_TYPE_COUNT; i++) g_em[i].have = 0;

    g_shader_vc = LoadShaderFromMemory(skin_vs_vc(), skin_fs_vc());
    int loc = GetShaderLocation(g_shader_vc, "lightDir");
    Vector3 ld = R3D_LIGHT_DIR;
    if (loc >= 0) SetShaderValue(g_shader_vc, loc, &ld, SHADER_UNIFORM_VEC3);
    g_loc_gain = GetShaderLocation(g_shader_vc, "gain");

    /* ── Modèles 3D des ennemis (assets/3d/3D_enemies/) ────────────────
       Cadrage : cible ≈ mi-hauteur du modèle, fovy ≈ hauteur×1.4 → taille
       écran cohérente. rest_phi : humanoïdes = 0 (regardent -Y → raylib +Z) ;
       véhicule LONG selon X = π/2. ENEMY_RUNNER n'a pas de modèle → sprite 2D.
       Valeurs à affiner au playtest. */
    #define BIPED_CAM(ty) ((Camera3D){ .position = {-3.6f, 3.2f, -4.4f}, \
        .target = {0.0f, (ty), 0.0f}, .up = {0,1,0}, .projection = CAMERA_ORTHOGRAPHIC })

    Camera3D cam_raider = BIPED_CAM(0.86f); cam_raider.fovy = 2.45f;
    load_enemy(ENEMY_RAIDER, "assets/3d/3D_enemies/voxel-raider.glb",
               cam_raider, 2.40f, 0.80f, 0.0f, 0.0f);

    Camera3D cam_brute = BIPED_CAM(0.81f); cam_brute.fovy = 2.70f;   /* large 2.3 */
    load_enemy(ENEMY_BRUTE, "assets/3d/3D_enemies/brute_rigged.glb",
               cam_brute, 2.55f, 0.80f, 0.0f, 0.0f);

    Camera3D cam_mutant = BIPED_CAM(0.89f); cam_mutant.fovy = 2.55f;
    load_enemy(ENEMY_MUTANT, "assets/3d/3D_enemies/mutant_rigged.glb",
               cam_mutant, 2.40f, 0.80f, 0.0f, 0.0f);

    Camera3D cam_ghost = BIPED_CAM(1.09f); cam_ghost.fovy = 2.40f;   /* flotte (ymin 0.28) */
    load_enemy(ENEMY_GHOST, "assets/3d/3D_enemies/wraith_rigged.glb",
               cam_ghost, 2.30f, 0.78f, 0.0f, 0.0f);

    Camera3D cam_pathbreaker = BIPED_CAM(0.90f); cam_pathbreaker.fovy = 2.55f;
    load_enemy(ENEMY_PATHBREAKER, "assets/3d/3D_enemies/PathBreaker.glb",
               cam_pathbreaker, 2.45f, 0.80f, 0.0f, 0.0f);

    Camera3D cam_healer = BIPED_CAM(0.99f); cam_healer.fovy = 2.80f;
    load_enemy(ENEMY_HEALER, "assets/3d/3D_enemies/plague_healer.glb",
               cam_healer, 2.50f, 0.82f, 0.0f, 0.0f);

    Camera3D cam_hunter = BIPED_CAM(0.81f); cam_hunter.fovy = 2.30f;
    load_enemy(ENEMY_HUNTER, "assets/3d/3D_enemies/Hunter_voxel_rigged.glb",
               cam_hunter, 2.30f, 0.80f, 0.0f, 0.0f);

    Camera3D cam_artillery = BIPED_CAM(0.89f); cam_artillery.fovy = 2.55f;
    load_enemy(ENEMY_ARTILLERY, "assets/3d/3D_enemies/heavy_gunner.glb",
               cam_artillery, 2.40f, 0.80f, 0.0f, 0.0f);

    Camera3D cam_runner = BIPED_CAM(0.88f); cam_runner.fovy = 2.45f;  /* goule véloce 1.75 */
    load_enemy(ENEMY_RUNNER, "assets/3d/3D_enemies/runner.glb",
               cam_runner, 2.35f, 0.80f, 0.0f, 0.0f);

    /* Blindé (wasteland_rig) : GROS véhicule 5.85×4×2.1, LONG selon X →
       regarde +X → rest_phi = π/2. Caméra reculée + fovy large. */
    Camera3D cam_vehicle = { .position = {-4.4f, 3.8f, -5.2f}, .target = {0.0f, 1.55f, 0.0f},
                             .up = {0,1,0}, .fovy = 5.2f, .projection = CAMERA_ORTHOGRAPHIC };
    load_enemy(ENEMY_VEHICLE, "assets/3d/3D_enemies/wasteland_rig.glb",
               cam_vehicle, 2.8f, 0.70f, 1.5708f, 0.0f);

    for (int i = 0; i < MAX_ENEMIES; i++) {
        g_rt[i].id = 0; g_used[i] = 0; g_rt_type[i] = -1; g_anim_t[i] = 0; g_anim_i[i] = -1;
        g_prev_x[i] = 0; g_prev_y[i] = 0; g_facing[i] = 0; g_first[i] = 1;
    }
    for (int i = 0; i < ENEMY_TYPE_COUNT; i++) if (g_em[i].have) g_loaded = 1;
}

void render3d_enemies_shutdown(void) {
    if (!g_loaded) return;
    for (int i = 0; i < MAX_ENEMIES; i++)
        if (g_rt[i].id != 0) UnloadRenderTexture(g_rt[i]);
    for (int t = 0; t < ENEMY_TYPE_COUNT; t++) {
        if (!g_em[t].have) continue;
        if (g_em[t].anims) UnloadModelAnimations(g_em[t].anims, g_em[t].anim_count);
        UnloadModel(g_em[t].model);
    }
    UnloadShader(g_shader_vc);
    g_loaded = 0;
}

int render3d_enemies_available(void) { return g_loaded; }

int render3d_enemy_has_model(int enemy_type) {
    return (enemy_type >= 0 && enemy_type < ENEMY_TYPE_COUNT && g_em[enemy_type].have);
}

static void ensure_rt(int i) {
    if (g_rt[i].id != 0) return;
    g_rt[i] = LoadRenderTexture(E_RT_W, E_RT_H);
}

void render3d_enemies_set_fog(Color col, float density) {
    if (!g_loaded) return;
    skin_set_fog(g_shader_vc, col, density);
}

void render3d_enemies_prepass(const EnemyPool *ep) {
    if (!g_loaded || ep == NULL) return;
    render3d_enemies_set_fog(BLANK, 0.0f);   /* mode 2D : pas de brouillard */
    float dt = GetFrameTime();

    for (int i = 0; i < MAX_ENEMIES; i++) {
        const Enemy *e = &ep->enemies[i];
        int has = (e->active && !e->dead && e->spawn_delay <= 0.0f &&
                   render3d_enemy_has_model(e->type));
        if (!has) { g_used[i] = 0; continue; }
        EnemyModel *em = &g_em[e->type];

        if (g_first[i]) { g_prev_x[i] = e->x; g_prev_y[i] = e->y;
                          g_facing[i] = 1.5708f; g_first[i] = 0; }
        float dx = e->x - g_prev_x[i], dy = e->y - g_prev_y[i];
        float moved2 = dx*dx + dy*dy;
        g_prev_x[i] = e->x; g_prev_y[i] = e->y;

        /* Orientation = direction de déplacement (lissée). */
        if (moved2 > E_MOVE_EPS2) {
            float desired = atan2f(dy, dx);
            g_facing[i] = angle_approach(g_facing[i], desired, E_TURN_SPEED * dt);
        }

        int aidx = (moved2 > E_MOVE_EPS2) ? em->a_walk : em->a_idle;
        if (g_anim_i[i] != aidx) { g_anim_i[i] = aidx; g_anim_t[i] = 0.0f; }
        g_anim_t[i] += dt;

        if (em->anim_count > 0) {                 /* modèle sans anim → statique */
            int   fc    = em->anims[aidx].keyframeCount;
            float frame = (fc > 0) ? fmodf(g_anim_t[i] * E_ANIM_FPS, (float)fc) : 0.0f;
            UpdateModelAnimation(em->model, em->anims[aidx], frame);
        }

        ensure_rt(i);
        BeginTextureMode(g_rt[i]);
            ClearBackground(BLANK);
            BeginMode3D(em->cam);
                rlDisableBackfaceCulling();       /* double-face (winding importé) */
                if (g_loc_gain >= 0)
                    SetShaderValue(g_shader_vc, g_loc_gain, &em->gain, SHADER_UNIFORM_FLOAT);
                float yaw = render3d_yaw_for_aim(em->cam, g_facing[i], em->rest_phi) + em->yaw_off;
                rlPushMatrix();
                    rlRotatef(yaw, 0, 1, 0);
                    DrawModel(em->model, (Vector3){0,0,0}, 1.0f, WHITE);
                rlPopMatrix();
                rlEnableBackfaceCulling();
            EndMode3D();
        EndTextureMode();
        g_used[i] = 1; g_rt_type[i] = e->type;
    }
}

Texture2D render3d_enemy_tex(int enemy_index) {
    if (!g_loaded || enemy_index < 0 || enemy_index >= MAX_ENEMIES || !g_used[enemy_index])
        return (Texture2D){0};
    return g_rt[enemy_index].texture;
}

Rectangle render3d_enemy_dst(int enemy_index, float x, float y, float size) {
    float ds = 2.4f, ya = 0.80f;
    if (enemy_index >= 0 && enemy_index < MAX_ENEMIES) {
        int t = g_rt_type[enemy_index];
        if (t >= 0 && t < ENEMY_TYPE_COUNT && g_em[t].have) {
            ds = g_em[t].dst_scale; ya = g_em[t].dst_yanchor;
        }
    }
    float box = size * 2.0f;
    float w   = box * ds;
    float h   = w * ((float)E_RT_H / (float)E_RT_W);
    return (Rectangle){ x - w*0.5f, y + box*0.5f - h*ya, w, h };
}

/* ════════════════════════════════════════════════════
   MODE HÉROS — dessin direct dans la scène 3D courante
   (pas de RenderTexture : anim + gain + orientation ici)
   ════════════════════════════════════════════════════ */
int render3d_enemies_draw_world(int type, Vector3 pos, float heading_rad,
                                float scale, int anim_kind, float anim_time,
                                int update_anim) {
    if (!g_loaded || type < 0 || type >= ENEMY_TYPE_COUNT) return 0;
    EnemyModel *em = &g_em[type];
    if (!em->have) return 0;

    int aidx = (anim_kind == 2) ? em->a_attack
             : (anim_kind == 1) ? em->a_walk : em->a_idle;
    if (update_anim && em->anim_count > 0 && aidx >= 0) {
        int fc = em->anims[aidx].keyframeCount;
        int fr = (fc > 0) ? (int)fmodf(anim_time * E_ANIM_FPS, (float)fc) : 0;
        UpdateModelAnimation(em->model, em->anims[aidx], fr);
    }
    if (g_loc_gain >= 0)
        SetShaderValue(g_shader_vc, g_loc_gain, &em->gain, SHADER_UNIFORM_FLOAT);

    rlDisableBackfaceCulling();
    rlPushMatrix();
        rlTranslatef(pos.x, pos.y, pos.z);
        rlRotatef((heading_rad - em->rest_phi) * RAD2DEG + em->yaw_off,
                  0.0f, 1.0f, 0.0f);
        rlScalef(scale, scale, scale);
        DrawModel(em->model, (Vector3){0.0f, 0.0f, 0.0f}, 1.0f, WHITE);
    rlPopMatrix();
    rlEnableBackfaceCulling();
    return 1;
}
