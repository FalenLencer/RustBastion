/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */
/* ════════════════════════════════════════════════════════════════
   ui/render3d_units.c — rendu 3D des unités (voir render3d_units.h).

   GLB skinné par type d'unité (table indexée UNIT_*). Animation CPU via
   UpdateModelAnimation (raylib déforme le maillage). Pré-passe : pour
   chaque unité 3D-capable, on choisit l'anim (état), on avance le temps,
   on déforme le modèle PARTAGÉ, on le rend dans la RT de l'instance, puis
   renderer.c blitte cette texture à la place du sprite (si g_units_3d).
   ════════════════════════════════════════════════════════════════ */
#include "render3d_units.h"
#include "render3d.h"          /* render3d_yaw_for_aim (caméra oblique) */
#include "render3d_skin.h"     /* helpers jumeaux units/enemies         */
#include "../combat/combat_math.h" /* angle_approach (source unique)    */
#include "rlgl.h"
#include <math.h>
#include <string.h>
#include <stddef.h>
#include <ctype.h>

/* ── Réglages ──────────────────────────────────────────────────────
   Cadrage, échelle, grain pixel art : AUTOMATIQUES (r3d_frame_auto,
   render3d_skin.h) d'après la boîte englobante de chaque modèle. */
#define U_YAW_OFF     0.0f          /* offset fin si l'unité vise à côté (deg) */
#define U_REST_PHI    0.0f          /* modèles pointent +Z au repos (rad)      */
#define U_ANIM_FPS    24.0f         /* cadence d'origine des anims (Blender) */
/* « En marche » = VITESSE (px/s), pas déplacement par image (l'ancien seuil
   par image figeait le Lourd dès 120 FPS, soldat/médic/ouvrier à 165). */
#define U_MOVE_SPEED  8.0f          /* px/s de simulation (≈ 0,2 case/s)    */
#define U_TURN_SPEED  10.0f         /* rad/s de simulation (vers cible/dir) */
/* Temps : horloge de SIMULATION (r3d_anim_step) ; marche calée sur la
   distance parcourue (r3d_stride_step, référence = vitesse de base du type). */

/* (Lumière : R3D_LIGHT_DIR, source unique dans render3d.h.)
   La caméra et le cadrage sont PAR TYPE, calculés (cf. UnitModel.fr). */

/* (Shaders vertex-color : skin_vs_vc/skin_fs_vc, render3d_skin.h.) */

/* ── Modèles par type d'unité (cadrage PROPRE à chaque type) ───────── */
typedef struct {
    int             have;
    Model           model;
    ModelAnimation *anims;
    int             anim_count;
    int             a_idle, a_walk, a_attack;   /* index par nom        */
    R3dFrame        fr;                           /* caméra + RT + pied    */
    float           base_size;                    /* rayon logique de réf. */
    float           ref_speed;                    /* px/s où la marche joue à 1× */
    float           rest_phi, yaw_off;            /* orientation PAR TYPE  */
    float           gain;                          /* normalisation luminosité */
} UnitModel;

static int             g_loaded = 0;
static Shader          g_shader_vc;
static int             g_loc_gain = -1;            /* uniform "gain" du shader */
static UnitModel       g_um[UNIT_TYPE_COUNT];
static UnitModel       g_hero_um;                  /* modèle DÉDIÉ du héros    */

static RenderTexture2D g_rt[MAX_UNITS];
static int             g_used[MAX_UNITS];
static int             g_rt_type[MAX_UNITS];     /* type d'unité rendu dans le RT */
static float           g_anim_t [MAX_UNITS];
static int             g_anim_i [MAX_UNITS];
static float           g_prev_x [MAX_UNITS];
static float           g_prev_y [MAX_UNITS];
static float           g_facing [MAX_UNITS];
static int             g_first  [MAX_UNITS];     /* 1 tant que pas initialisé */
static double          g_clock_last = -1.0;      /* cf. r3d_anim_step         */

/* (Matcher d'anims : skin_anim_match, render3d_skin.h.) */

/* (Application du shader : skin_apply_vc, render3d_skin.h.) */

/* (Gain de luminance : skin_model_gain, render3d_skin.h.) */

/* base_size : rayon logique du type (UNIT_BASE_STATS) → échelle et cadrage
   automatiques ; 0 = modèle dessiné en direct seulement (héros). */
static void load_um(UnitModel *um, const char *path, float base_size,
                    float rest_phi, float yaw_off) {
    um->model = LoadModel(path);
    if (um->model.meshCount == 0) { um->have = 0; return; }
    if (!skin_model_indices_ok(&um->model, path)) {
        UnloadModel(um->model); um->have = 0; return;
    }
    skin_apply_vc(&um->model, g_shader_vc);
    um->gain = skin_model_gain(&um->model);
    um->anims = LoadModelAnimations(path, &um->anim_count);   /* 0 = modèle statique */
    static const char *KW_IDLE[]   = {"idle","stand","rest"};
    static const char *KW_WALK[]   = {"walk","run","move"};
    static const char *KW_ATTACK[] = {"attack","fight","mine","dig","hit","strike",
                                      "shoot","punch","slam","bite","melee","work"};
    int ai = skin_anim_match(um->anims, um->anim_count, KW_IDLE,   (int)(sizeof(KW_IDLE)  /sizeof(KW_IDLE[0])));
    int aw = skin_anim_match(um->anims, um->anim_count, KW_WALK,   (int)(sizeof(KW_WALK)  /sizeof(KW_WALK[0])));
    int aa = skin_anim_match(um->anims, um->anim_count, KW_ATTACK, (int)(sizeof(KW_ATTACK)/sizeof(KW_ATTACK[0])));
    um->a_idle   = (ai >= 0) ? ai : 0;            /* défaut : 1ʳᵉ anim         */
    um->a_walk   = (aw >= 0) ? aw : um->a_idle;   /* pas de walk → idle        */
    um->a_attack = (aa >= 0) ? aa : um->a_idle;   /* pas d'attaque → idle      */
    um->base_size = base_size;
    if (base_size > 0.0f) {
        BoundingBox bb = GetModelBoundingBox(um->model);
        um->fr = r3d_frame_auto(bb, r3d_ent_px_per_unit(bb, base_size));
    }
    um->rest_phi = rest_phi; um->yaw_off = yaw_off;
    um->have = 1;
}

static void load_unit(int type, const char *path, float rest_phi) {
    load_um(&g_um[type], path, UNIT_BASE_STATS[type].size, rest_phi, U_YAW_OFF);
    g_um[type].ref_speed = UNIT_BASE_STATS[type].speed * TILE_SIZE;  /* cases/s → px/s */
}

void render3d_units_init(void) {
    g_loaded = 0;
    for (int i = 0; i < UNIT_TYPE_COUNT; i++) g_um[i].have = 0;

    g_shader_vc = LoadShaderFromMemory(skin_vs_vc(), skin_fs_vc());
    int loc = GetShaderLocation(g_shader_vc, "lightDir");
    Vector3 ld = R3D_LIGHT_DIR;
    if (loc >= 0) SetShaderValue(g_shader_vc, loc, &ld, SHADER_UNIFORM_VEC3);
    g_loc_gain = GetShaderLocation(g_shader_vc, "gain");

    /* ── Modèles 3D PROPRES (assets/3d/3D_Troupes/) ───────────────────
       Cadrage/échelle automatiques (taille = UNIT_BASE_STATS.size).
       rest_phi = angle sol de l'avant au repos (raylib) : +Z=0, +X=π/2.
       Humanoïdes : regardent -Y Blender → raylib +Z (0). Le chien est LONG
       selon X (3,4 × 0,76) → regarde +X → π/2. */
    load_unit(UNIT_SOLDIER, "assets/3d/3D_Troupes/gas_mask_soldier.glb",  0.0f);
    load_unit(UNIT_HEAVY,   "assets/3d/3D_Troupes/iron_juggernaut.glb",   0.0f);
    load_unit(UNIT_MEDIC,   "assets/3d/3D_Troupes/field_medic_voxel.glb", 0.0f);
    load_unit(UNIT_DOG,     "assets/3d/3D_Troupes/spiked-hound.glb",      1.5708f);
    load_unit(UNIT_WORKER,  "assets/3d/3D_Troupes/ouvrier.glb",           0.0f);

    /* HÉROS (mode héros) : ~1.55 de haut, manteau + casque + fusil.
       Dessiné en DIRECT dans la scène (jamais de RT → pas de cadrage).
       Anims : Idle / Run / Shoot. */
    memset(&g_hero_um, 0, sizeof(g_hero_um));
    load_um(&g_hero_um, "assets/3d/3D_Troupes/hero.glb", 0.0f, 0.0f, 0.0f);

    g_clock_last = -1.0;
    for (int i = 0; i < MAX_UNITS; i++) {
        g_rt[i].id = 0; g_used[i] = 0; g_rt_type[i] = -1; g_anim_t[i] = 0; g_anim_i[i] = -1;
        g_prev_x[i] = 0; g_prev_y[i] = 0; g_facing[i] = 0; g_first[i] = 1;
    }
    for (int i = 0; i < UNIT_TYPE_COUNT; i++) if (g_um[i].have) g_loaded = 1;
    if (g_hero_um.have) g_loaded = 1;
}

void render3d_units_shutdown(void) {
    if (!g_loaded) return;
    for (int i = 0; i < MAX_UNITS; i++)
        if (g_rt[i].id != 0) UnloadRenderTexture(g_rt[i]);
    for (int t = 0; t < UNIT_TYPE_COUNT; t++) {
        if (!g_um[t].have) continue;
        if (g_um[t].anims) UnloadModelAnimations(g_um[t].anims, g_um[t].anim_count);
        UnloadModel(g_um[t].model);
    }
    if (g_hero_um.have) {
        if (g_hero_um.anims)
            UnloadModelAnimations(g_hero_um.anims, g_hero_um.anim_count);
        UnloadModel(g_hero_um.model);
        g_hero_um.have = 0;
    }
    UnloadShader(g_shader_vc);
    g_loaded = 0;
}

int render3d_units_available(void) { return g_loaded; }

int render3d_unit_has_model(int unit_type) {
    return (unit_type >= 0 && unit_type < UNIT_TYPE_COUNT && g_um[unit_type].have);
}

void render3d_units_set_fog(Color col, float density) {
    if (!g_loaded) return;
    skin_set_fog(g_shader_vc, col, density);
}

void render3d_units_prepass(const UnitPool *up, const EnemyPool *ep) {
    if (!g_loaded || up == NULL) return;
    render3d_units_set_fog(BLANK, 0.0f);   /* mode 2D : pas de brouillard */
    float dt = r3d_anim_step(&g_clock_last);   /* temps de SIMULATION */

    for (int i = 0; i < MAX_UNITS; i++) {
        const Unit *u = &up->units[i];
        int has = (u->active && render3d_unit_has_model(u->type));
        /* Slot libéré → réinitialisé : une unité qui le reprend ne « marche »
           pas une frame depuis la position de la précédente. */
        if (!has) { g_used[i] = 0; g_first[i] = 1; continue; }
        UnitModel *um = &g_um[u->type];

        if (g_first[i]) { g_prev_x[i] = u->x; g_prev_y[i] = u->y;
                          g_facing[i] = 1.5708f; g_first[i] = 0; g_anim_i[i] = -1; }
        float dx = u->x - g_prev_x[i], dy = u->y - g_prev_y[i];
        float dist   = sqrtf(dx*dx + dy*dy);
        int   moving = (dt > 1e-5f) && (dist > U_MOVE_SPEED * dt);
        g_prev_x[i] = u->x; g_prev_y[i] = u->y;

        /* Orientation désirée : vers la CIBLE si elle existe (attaque/poursuite),
           sinon vers le DÉPLACEMENT. Rotation lissée (turn rate). */
        float desired = g_facing[i];
        if (ep && u->target_idx >= 0 && u->target_idx < MAX_ENEMIES &&
            ep->enemies[u->target_idx].active) {
            const Enemy *e = &ep->enemies[u->target_idx];
            desired = atan2f(e->y - u->y, e->x - u->x);
        } else if (moving) {
            desired = atan2f(dy, dx);
        }
        g_facing[i] = angle_approach(g_facing[i], desired, U_TURN_SPEED * dt);

        /* Choix de l'animation selon l'état/mouvement. Jeu figé (pause…) :
           on GARDE l'anim en cours (sinon la marche retomberait en repos). */
        int aidx;
        if (dt <= 0.0f && g_anim_i[i] >= 0) aidx = g_anim_i[i];
        else if (u->state == USTATE_ATTACK)  aidx = um->a_attack;
        else if (moving)                     aidx = um->a_walk;
        else                                 aidx = um->a_idle;

        if (g_anim_i[i] != aidx) { g_anim_i[i] = aidx; g_anim_t[i] = 0.0f; }
        g_anim_t[i] += (moving && aidx == um->a_walk)
                     ? r3d_stride_step(dist, um->ref_speed, dt) : dt;

        /* Modèle animé → joue l'anim (frame FLOTTANT, raylib interpole, bouclage).
           Modèle SANS animation → reste en pose de repos (on saute l'update,
           sinon déréférencement d'un tableau d'anims vide). */
        if (um->anim_count > 0) {
            int   fc    = um->anims[aidx].keyframeCount;
            float frame = (fc > 0) ? fmodf(g_anim_t[i] * U_ANIM_FPS, (float)fc) : 0.0f;
            UpdateModelAnimation(um->model, um->anims[aidx], frame);
        }

        r3d_ent_ensure_rt(&g_rt[i], um->fr.rt_w, um->fr.rt_h);
        BeginTextureMode(g_rt[i]);
            ClearBackground(BLANK);
            BeginMode3D(um->fr.cam);
                /* Rendu DOUBLE-FACE : certains modèles importés ont un winding
                   de faces incohérent → le backface culling « troue » le
                   maillage (effet semi-transparent, ex. iron_juggernaut). Les
                   NORMAL du GLB restent corrects donc l'éclairage est bon. */
                rlDisableBackfaceCulling();
                if (g_loc_gain >= 0)
                    SetShaderValue(g_shader_vc, g_loc_gain, &um->gain, SHADER_UNIFORM_FLOAT);
                float yaw = render3d_yaw_for_aim(um->fr.cam, g_facing[i], um->rest_phi) + um->yaw_off;
                rlPushMatrix();
                    rlRotatef(yaw, 0, 1, 0);
                    DrawModel(um->model, (Vector3){0,0,0}, 1.0f, WHITE);
                rlPopMatrix();
                rlEnableBackfaceCulling();
            EndMode3D();
        EndTextureMode();
        g_used[i] = 1; g_rt_type[i] = u->type;
    }
}

Texture2D render3d_unit_tex(int unit_index) {
    if (!g_loaded || unit_index < 0 || unit_index >= MAX_UNITS || !g_used[unit_index])
        return (Texture2D){0};
    return g_rt[unit_index].texture;
}

static const UnitModel *um_of_slot(int unit_index) {
    if (unit_index < 0 || unit_index >= MAX_UNITS) return NULL;
    int t = g_rt_type[unit_index];
    return (t >= 0 && t < UNIT_TYPE_COUNT && g_um[t].have) ? &g_um[t] : NULL;
}

Rectangle render3d_unit_dst(int unit_index, float cx, float cy, float size) {
    const UnitModel *um = um_of_slot(unit_index);
    if (!um) return (Rectangle){ cx, cy, 0, 0 };
    return r3d_ent_dst(&um->fr, cx, cy, size, um->base_size);
}

float render3d_unit_head_y(int unit_index, Rectangle dst) {
    const UnitModel *um = um_of_slot(unit_index);
    if (!um) return dst.y;
    return dst.y + um->fr.head.y * dst.height / (float)um->fr.rt_h;
}

/* ════════════════════════════════════════════════════
   MODE HÉROS — dessin direct dans la scène 3D courante
   (pas de RenderTexture : anim + gain + orientation ici)
   ════════════════════════════════════════════════════ */
static int draw_um_world(UnitModel *um, Vector3 pos, float heading_rad,
                         float scale, int anim_kind, float anim_time,
                         int update_anim) {
    if (!um->have) return 0;

    int aidx = (anim_kind == 2) ? um->a_attack
             : (anim_kind == 1) ? um->a_walk : um->a_idle;
    if (update_anim && um->anim_count > 0 && aidx >= 0) {
        int fc = um->anims[aidx].keyframeCount;
        int fr = (fc > 0) ? (int)fmodf(anim_time * U_ANIM_FPS, (float)fc) : 0;
        UpdateModelAnimation(um->model, um->anims[aidx], fr);
    }
    if (g_loc_gain >= 0)
        SetShaderValue(g_shader_vc, g_loc_gain, &um->gain, SHADER_UNIFORM_FLOAT);

    rlDisableBackfaceCulling();
    rlPushMatrix();
        rlTranslatef(pos.x, pos.y, pos.z);
        rlRotatef((heading_rad - um->rest_phi) * RAD2DEG + um->yaw_off,
                  0.0f, 1.0f, 0.0f);
        rlScalef(scale, scale, scale);
        DrawModel(um->model, (Vector3){0.0f, 0.0f, 0.0f}, 1.0f, WHITE);
    rlPopMatrix();
    rlEnableBackfaceCulling();
    return 1;
}

int render3d_units_draw_world(int type, Vector3 pos, float heading_rad,
                              float scale, int anim_kind, float anim_time,
                              int update_anim) {
    if (!g_loaded || type < 0 || type >= UNIT_TYPE_COUNT) return 0;
    return draw_um_world(&g_um[type], pos, heading_rad, scale,
                         anim_kind, anim_time, update_anim);
}

int render3d_hero_draw_world(Vector3 pos, float heading_rad, float scale,
                             int anim_kind, float anim_time,
                             int update_anim) {
    if (!g_loaded) return 0;
    return draw_um_world(&g_hero_um, pos, heading_rad, scale,
                         anim_kind, anim_time, update_anim);
}
