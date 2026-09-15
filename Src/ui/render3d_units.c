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

/* ── Réglages (aucun rendu visible côté outil → à régler en jeu) ── */
#define U_RT_W        192
#define U_RT_H        240
#define U_YAW_OFF     0.0f          /* offset fin si l'unité vise à côté (deg) */
#define U_REST_PHI    0.0f          /* modèles pointent +Z au repos (rad)      */
#define U_ANIM_FPS    24.0f         /* cadence de lecture des anims (Blender)*/
#define U_MOVE_EPS2   0.40f         /* seuil² (px) pour considérer « bouge »*/
#define U_FACE_EPS2   0.30f         /* seuil² pour rafraîchir l'orientation */
#define U_TURN_SPEED  10.0f         /* vitesse de rotation vers cible/dir (rad/s)*/

/* (Lumière : R3D_LIGHT_DIR, source unique dans render3d.h.)
   La caméra et le cadrage (dst) sont PAR TYPE (cf. UnitModel/load_unit). */

/* (Shaders vertex-color : skin_vs_vc/skin_fs_vc, render3d_skin.h.) */

/* ── Modèles par type d'unité (cadrage PROPRE à chaque type) ───────── */
typedef struct {
    int             have;
    Model           model;
    ModelAnimation *anims;
    int             anim_count;
    int             a_idle, a_walk, a_attack;   /* index par nom        */
    Camera3D        cam;                          /* caméra de rendu      */
    float           dst_scale, dst_yanchor;       /* blit sur la case     */
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

/* (Matcher d'anims : skin_anim_match, render3d_skin.h.) */

/* (Application du shader : skin_apply_vc, render3d_skin.h.) */

/* (Gain de luminance : skin_model_gain, render3d_skin.h.) */

static void load_um(UnitModel *um, const char *path, Camera3D cam,
                    float dst_scale, float dst_yanchor,
                    float rest_phi, float yaw_off) {
    um->model = LoadModel(path);
    if (um->model.meshCount == 0) { um->have = 0; return; }
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
    um->cam = cam; um->dst_scale = dst_scale; um->dst_yanchor = dst_yanchor;
    um->rest_phi = rest_phi; um->yaw_off = yaw_off;
    um->have = 1;
}

static void load_unit(int type, const char *path, Camera3D cam,
                      float dst_scale, float dst_yanchor,
                      float rest_phi, float yaw_off) {
    load_um(&g_um[type], path, cam, dst_scale, dst_yanchor,
            rest_phi, yaw_off);
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
       Les anciennes versions sont archivées dans assets/3d/test/.
       Les unités sans modèle propre (soldat/médic/ouvrier) retombent sur
       leur SPRITE 2D. NB : ces GLB sont SANS animation → pose de repos.
       Signature : load_unit(type, path, cam, dst_scale, dst_yanchor,
                              rest_phi, yaw_off).  rest_phi = angle sol de
       l'avant au repos (raylib) : +Z=0, +X=π/2, -X=-π/2, -Z=π. */

    /* Caméra 3/4 commune aux bipèdes ; on ne fait varier que la CIBLE
       (mi-hauteur du modèle) et le FOVY (≈ hauteur × 1.4) pour que chaque
       modèle remplisse son RT de la même façon → taille écran cohérente.
       rest_phi = 0 (les humanoïdes du user regardent l'axe -Y → raylib +Z). */
    #define BIPED_CAM(ty)  ((Camera3D){ .position = {-3.4f, 3.4f, -4.4f}, \
        .target = {0.0f, (ty), 0.0f}, .up = {0,1,0}, .projection = CAMERA_ORTHOGRAPHIC })

    /* Soldat (gas_mask_soldier) : ~1.08 de haut. */
    Camera3D cam_sold = BIPED_CAM(0.54f); cam_sold.fovy = 1.55f;
    load_unit(UNIT_SOLDIER, "assets/3d/3D_Troupes/gas_mask_soldier.glb",
              cam_sold, 2.5f, 0.84f, 0.0f, 0.0f);

    /* Lourd (iron_juggernaut) : ~1.85 de haut, trapu. */
    Camera3D cam_heavy = BIPED_CAM(0.95f); cam_heavy.fovy = 2.6f;
    load_unit(UNIT_HEAVY, "assets/3d/3D_Troupes/iron_juggernaut.glb",
              cam_heavy, 2.5f, 0.84f, 0.0f, 0.0f);

    /* Médic (field_medic_voxel) : ~1.38 de haut. */
    Camera3D cam_medic = BIPED_CAM(0.67f); cam_medic.fovy = 1.95f;
    load_unit(UNIT_MEDIC, "assets/3d/3D_Troupes/field_medic_voxel.glb",
              cam_medic, 2.5f, 0.84f, 0.0f, 0.0f);

    /* Chien (spiked-hound) : LONG selon l'axe X (3.4×0.76) → regarde +X au
       repos → rest_phi = π/2. Cadre large + bas. */
    Camera3D cam_dog = { .position = {-3.4f, 3.2f, -4.4f}, .target = {0.0f, 0.65f, 0.0f},
                         .up = {0,1,0}, .fovy = 3.6f, .projection = CAMERA_ORTHOGRAPHIC };
    load_unit(UNIT_DOG, "assets/3d/3D_Troupes/spiked-hound.glb",
              cam_dog, 2.0f, 0.72f, 1.5708f, 0.0f);

    /* Ouvrier (ouvrier) : ~1.97 de haut (le plus grand). */
    Camera3D cam_worker = BIPED_CAM(0.98f); cam_worker.fovy = 2.80f;
    load_unit(UNIT_WORKER, "assets/3d/3D_Troupes/ouvrier.glb",
              cam_worker, 2.5f, 0.84f, 0.0f, 0.0f);

    /* HÉROS (mode héros) : ~1.55 de haut, manteau + casque + fusil.
       Dessiné en DIRECT dans la scène (jamais de RT) — la caméra sert
       juste de gabarit. Anims : Idle / Run / Shoot. */
    memset(&g_hero_um, 0, sizeof(g_hero_um));
    Camera3D cam_hero = BIPED_CAM(0.78f); cam_hero.fovy = 2.2f;
    load_um(&g_hero_um, "assets/3d/3D_Troupes/hero.glb",
            cam_hero, 2.5f, 0.84f, 0.0f, 0.0f);

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

static void ensure_rt(int i) {
    if (g_rt[i].id != 0) return;
    g_rt[i] = LoadRenderTexture(U_RT_W, U_RT_H);
}

void render3d_units_set_fog(Color col, float density) {
    if (!g_loaded) return;
    skin_set_fog(g_shader_vc, col, density);
}

void render3d_units_prepass(const UnitPool *up, const EnemyPool *ep) {
    if (!g_loaded || up == NULL) return;
    render3d_units_set_fog(BLANK, 0.0f);   /* mode 2D : pas de brouillard */
    float dt = GetFrameTime();

    for (int i = 0; i < MAX_UNITS; i++) {
        const Unit *u = &up->units[i];
        int has = (u->active && render3d_unit_has_model(u->type));
        if (!has) { g_used[i] = 0; continue; }
        UnitModel *um = &g_um[u->type];

        if (g_first[i]) { g_prev_x[i] = u->x; g_prev_y[i] = u->y;
                          g_facing[i] = 1.5708f; g_first[i] = 0; }
        float dx = u->x - g_prev_x[i], dy = u->y - g_prev_y[i];
        float moved2 = dx*dx + dy*dy;
        g_prev_x[i] = u->x; g_prev_y[i] = u->y;

        /* Orientation désirée : vers la CIBLE si elle existe (attaque/poursuite),
           sinon vers le DÉPLACEMENT. Rotation lissée (turn rate). */
        float desired = g_facing[i];
        if (ep && u->target_idx >= 0 && u->target_idx < MAX_ENEMIES &&
            ep->enemies[u->target_idx].active) {
            const Enemy *e = &ep->enemies[u->target_idx];
            desired = atan2f(e->y - u->y, e->x - u->x);
        } else if (moved2 > U_FACE_EPS2) {
            desired = atan2f(dy, dx);
        }
        g_facing[i] = angle_approach(g_facing[i], desired, U_TURN_SPEED * dt);

        /* Choix de l'animation selon l'état/mouvement. */
        int aidx;
        if (u->state == USTATE_ATTACK)     aidx = um->a_attack;
        else if (moved2 > U_MOVE_EPS2)     aidx = um->a_walk;
        else                               aidx = um->a_idle;

        if (g_anim_i[i] != aidx) { g_anim_i[i] = aidx; g_anim_t[i] = 0.0f; }
        g_anim_t[i] += dt;

        /* Modèle animé → joue l'anim (frame FLOTTANT, raylib interpole, bouclage).
           Modèle SANS animation → reste en pose de repos (on saute l'update,
           sinon déréférencement d'un tableau d'anims vide). */
        if (um->anim_count > 0) {
            int   fc    = um->anims[aidx].keyframeCount;
            float frame = (fc > 0) ? fmodf(g_anim_t[i] * U_ANIM_FPS, (float)fc) : 0.0f;
            UpdateModelAnimation(um->model, um->anims[aidx], frame);
        }

        ensure_rt(i);
        BeginTextureMode(g_rt[i]);
            ClearBackground(BLANK);
            BeginMode3D(um->cam);
                /* Rendu DOUBLE-FACE : certains modèles importés ont un winding
                   de faces incohérent → le backface culling « troue » le
                   maillage (effet semi-transparent, ex. iron_juggernaut). Les
                   NORMAL du GLB restent corrects donc l'éclairage est bon. */
                rlDisableBackfaceCulling();
                if (g_loc_gain >= 0)
                    SetShaderValue(g_shader_vc, g_loc_gain, &um->gain, SHADER_UNIFORM_FLOAT);
                float yaw = render3d_yaw_for_aim(um->cam, g_facing[i], um->rest_phi) + um->yaw_off;
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

Rectangle render3d_unit_dst(int unit_index, float cx, float cy, float size) {
    float ds = 2.6f, ya = 0.86f;             /* défauts (type inconnu)        */
    if (unit_index >= 0 && unit_index < MAX_UNITS) {
        int t = g_rt_type[unit_index];
        if (t >= 0 && t < UNIT_TYPE_COUNT && g_um[t].have) {
            ds = g_um[t].dst_scale; ya = g_um[t].dst_yanchor;
        }
    }
    float box = size * 2.0f;                 /* gabarit ≈ diamètre du sprite */
    float w   = box * ds;
    float h   = w * ((float)U_RT_H / (float)U_RT_W);
    return (Rectangle){ cx - w*0.5f, cy + box*0.5f - h*ya, w, h };
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
