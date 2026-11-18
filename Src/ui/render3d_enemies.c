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
#include "entity_fx.h"         /* ent_enemy_target (cible d'attaque)    */
#include "../combat/combat_math.h" /* angle_approach (source unique)    */
#include "rlgl.h"
#include <math.h>
#include <string.h>
#include <stddef.h>
#include <ctype.h>

/* ── Réglages ──────────────────────────────────────────────────────
   Cadrage, échelle, grain pixel art : AUTOMATIQUES (r3d_frame_auto). */
#define E_YAW_OFF     0.0f
#define E_ANIM_FPS    24.0f
/* « En marche » = VITESSE (px/s), pas déplacement par image : l'ancien seuil
   de 0,55 px/image figeait en pose de repos (et sans jamais tourner) le
   blindé et l'artillerie dès 60 FPS, brute et raider à 165 FPS. */
#define E_MOVE_SPEED  8.0f          /* px/s de simulation (≈ 0,2 case/s)       */
#define E_TURN_SPEED  12.0f         /* rad/s de simulation vers la direction   */
#define E_ATTACK_HOLD 0.55f         /* anim d'attaque tenue après un coup (s)  */
/* Temps : horloge de SIMULATION (r3d_anim_step) ; marche calée sur la
   distance parcourue (r3d_stride_step, référence = vitesse de base du type). */
#define E_TIMER_JUMP  0.02f         /* remontée de minuteur = un coup porté    */

/* (Lumière : R3D_LIGHT_DIR, source unique dans render3d.h.) */

/* (Shaders vertex-color : skin_vs_vc/skin_fs_vc, render3d_skin.h.) */

/* ── Modèle par type d'ennemi (cadrage PROPRE à chaque type) ──────── */
typedef struct {
    int             have;
    Model           model;
    ModelAnimation *anims;
    int             anim_count;
    int             a_idle, a_walk, a_attack;
    R3dFrame        fr;                            /* caméra + RT + pied      */
    float           base_size;                     /* rayon logique de réf.   */
    float           ref_speed;                     /* px/s où la marche joue à 1× */
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
static float           g_atk    [MAX_ENEMIES];   /* fenêtre d'attaque restante (s) */
static float           g_prev_at[MAX_ENEMIES];   /* atk_timer précédent            */
static float           g_prev_ar[MAX_ENEMIES];   /* arty_timer précédent           */
static double          g_clock_last = -1.0;      /* cf. r3d_anim_step              */

/* (Matcher d'anims : skin_anim_match, render3d_skin.h.) */

/* (Application du shader : skin_apply_vc, render3d_skin.h.) */

/* (Gain de luminance : skin_model_gain, render3d_skin.h.) */

/* Cadrage/échelle automatiques (taille = ENEMY_BASE_STATS.size). */
static void load_enemy(int type, const char *path, float rest_phi) {
    const float yaw_off = E_YAW_OFF;
    EnemyModel *em = &g_em[type];
    em->model = LoadModel(path);
    if (em->model.meshCount == 0) { em->have = 0; return; }
    if (!skin_model_indices_ok(&em->model, path)) {
        UnloadModel(em->model); em->have = 0; return;
    }
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
    em->base_size = ENEMY_BASE_STATS[type].size;
    em->ref_speed = ENEMY_BASE_STATS[type].speed * TILE_SIZE;   /* cases/s → px/s */
    BoundingBox bb = GetModelBoundingBox(em->model);
    em->fr = r3d_frame_auto(bb, r3d_ent_px_per_unit(bb, em->base_size));
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
       Cadrage/échelle automatiques. rest_phi : humanoïdes = 0 (regardent
       -Y Blender → raylib +Z) ; véhicule LONG selon X → +X → π/2. Le
       spectre flotte (ymin 0,28) : son pied reste au sol, il plane au-dessus
       de son ombre. */
    load_enemy(ENEMY_RAIDER,      "assets/3d/3D_enemies/voxel-raider.glb",        0.0f);
    load_enemy(ENEMY_BRUTE,       "assets/3d/3D_enemies/brute_rigged.glb",        0.0f);
    load_enemy(ENEMY_MUTANT,      "assets/3d/3D_enemies/mutant_rigged.glb",       0.0f);
    load_enemy(ENEMY_GHOST,       "assets/3d/3D_enemies/wraith_rigged.glb",       0.0f);
    load_enemy(ENEMY_PATHBREAKER, "assets/3d/3D_enemies/PathBreaker.glb",         0.0f);
    load_enemy(ENEMY_HEALER,      "assets/3d/3D_enemies/plague_healer.glb",       0.0f);
    load_enemy(ENEMY_HUNTER,      "assets/3d/3D_enemies/Hunter_voxel_rigged.glb", 0.0f);
    load_enemy(ENEMY_ARTILLERY,   "assets/3d/3D_enemies/heavy_gunner.glb",        0.0f);
    load_enemy(ENEMY_RUNNER,      "assets/3d/3D_enemies/runner.glb",              0.0f);
    load_enemy(ENEMY_VEHICLE,     "assets/3d/3D_enemies/wasteland_rig.glb",       1.5708f);

    g_clock_last = -1.0;
    for (int i = 0; i < MAX_ENEMIES; i++) {
        g_rt[i].id = 0; g_used[i] = 0; g_rt_type[i] = -1; g_anim_t[i] = 0; g_anim_i[i] = -1;
        g_prev_x[i] = 0; g_prev_y[i] = 0; g_facing[i] = 0; g_first[i] = 1;
        g_atk[i] = 0; g_prev_at[i] = 0; g_prev_ar[i] = 0;
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

void render3d_enemies_set_fog(Color col, float density) {
    if (!g_loaded) return;
    skin_set_fog(g_shader_vc, col, density);
}

void render3d_enemies_prepass(const EnemyPool *ep, const UnitPool *up,
                              const TowerPool *tp) {
    if (!g_loaded || ep == NULL) return;
    render3d_enemies_set_fog(BLANK, 0.0f);   /* mode 2D : pas de brouillard */
    float dt = r3d_anim_step(&g_clock_last);   /* temps de SIMULATION */

    for (int i = 0; i < MAX_ENEMIES; i++) {
        const Enemy *e = &ep->enemies[i];
        int has = (e->active && !e->dead && e->spawn_delay <= 0.0f &&
                   render3d_enemy_has_model(e->type));
        if (!has) { g_used[i] = 0; g_first[i] = 1; continue; }
        EnemyModel *em = &g_em[e->type];

        if (g_first[i]) { g_prev_x[i] = e->x; g_prev_y[i] = e->y;
                          g_facing[i] = 1.5708f; g_first[i] = 0; g_atk[i] = 0.0f;
                          g_anim_i[i] = -1;
                          g_prev_at[i] = e->atk_timer; g_prev_ar[i] = e->arty_timer; }
        float dx = e->x - g_prev_x[i], dy = e->y - g_prev_y[i];
        float dist   = sqrtf(dx*dx + dy*dy);
        int   moving = (dt > 1e-5f) && (dist > E_MOVE_SPEED * dt);
        g_prev_x[i] = e->x; g_prev_y[i] = e->y;

        /* Coup porté = un minuteur d'attaque REMONTE → anim d'attaque tenue
           E_ATTACK_HOLD s, l'ennemi FAIT FACE à sa cible pendant ce temps. */
        if (e->atk_timer  > g_prev_at[i] + E_TIMER_JUMP ||
            e->arty_timer > g_prev_ar[i] + E_TIMER_JUMP) g_atk[i] = E_ATTACK_HOLD;
        g_prev_at[i] = e->atk_timer; g_prev_ar[i] = e->arty_timer;
        g_atk[i] = fmaxf(0.0f, g_atk[i] - dt);

        /* Orientation : vers la cible en combat, sinon direction de marche. */
        float tx, ty;
        if (g_atk[i] > 0.0f && ent_enemy_target(e, up, tp, &tx, &ty)) {
            g_facing[i] = angle_approach(g_facing[i], atan2f(ty - e->y, tx - e->x),
                                         E_TURN_SPEED * dt);
        } else if (moving) {
            float desired = atan2f(dy, dx);
            g_facing[i] = angle_approach(g_facing[i], desired, E_TURN_SPEED * dt);
        }

        /* Jeu figé (pause…) : on GARDE l'anim en cours (pas de retombée en repos). */
        int aidx = (dt <= 0.0f && g_anim_i[i] >= 0) ? g_anim_i[i]
                 : (g_atk[i] > 0.0f) ? em->a_attack
                 : moving            ? em->a_walk : em->a_idle;
        if (g_anim_i[i] != aidx) { g_anim_i[i] = aidx; g_anim_t[i] = 0.0f; }
        g_anim_t[i] += (moving && aidx == em->a_walk)
                     ? r3d_stride_step(dist, em->ref_speed, dt) : dt;

        if (em->anim_count > 0) {                 /* modèle sans anim → statique */
            int   fc    = em->anims[aidx].keyframeCount;
            float frame = (fc > 0) ? fmodf(g_anim_t[i] * E_ANIM_FPS, (float)fc) : 0.0f;
            UpdateModelAnimation(em->model, em->anims[aidx], frame);
        }

        r3d_ent_ensure_rt(&g_rt[i], em->fr.rt_w, em->fr.rt_h);
        BeginTextureMode(g_rt[i]);
            ClearBackground(BLANK);
            BeginMode3D(em->fr.cam);
                rlDisableBackfaceCulling();       /* double-face (winding importé) */
                if (g_loc_gain >= 0)
                    SetShaderValue(g_shader_vc, g_loc_gain, &em->gain, SHADER_UNIFORM_FLOAT);
                float yaw = render3d_yaw_for_aim(em->fr.cam, g_facing[i], em->rest_phi) + em->yaw_off;
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

static const EnemyModel *em_of_slot(int enemy_index) {
    if (enemy_index < 0 || enemy_index >= MAX_ENEMIES) return NULL;
    int t = g_rt_type[enemy_index];
    return (t >= 0 && t < ENEMY_TYPE_COUNT && g_em[t].have) ? &g_em[t] : NULL;
}

Rectangle render3d_enemy_dst(int enemy_index, float x, float y, float size) {
    const EnemyModel *em = em_of_slot(enemy_index);
    if (!em) return (Rectangle){ x, y, 0, 0 };
    return r3d_ent_dst(&em->fr, x, y, size, em->base_size);
}

float render3d_enemy_head_y(int enemy_index, Rectangle dst) {
    const EnemyModel *em = em_of_slot(enemy_index);
    if (!em) return dst.y;
    return dst.y + em->fr.head.y * dst.height / (float)em->fr.rt_h;
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
