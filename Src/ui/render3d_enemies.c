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
#include "render3d.h"          /* render3d_yaw_for_aim (visée caméra oblique) */
#include "rlgl.h"
#include <math.h>
#include <string.h>
#include <stddef.h>

/* ── Réglages (à régler en jeu : aucun affichage côté outil) ─────── */
#define E_RT_W        160
#define E_RT_H        200
#define E_YAW_OFF     0.0f
#define E_REST_PHI    0.0f          /* modèles regardent +Z (raylib) au repos */
#define E_ANIM_FPS    24.0f
#define E_MOVE_EPS2   0.30f         /* seuil² (px) « bouge »                   */
#define E_TURN_SPEED  12.0f         /* vitesse de rotation vers la direction   */

static const Vector3 E_LIGHT = { -0.45f, -0.80f, -0.40f };

/* ── Shader vertex-color + éclairage directionnel (comme les unités) ── */
static const char *VS_VC =
"#version 330\n"
"in vec3 vertexPosition;\n"
"in vec3 vertexNormal;\n"
"in vec4 vertexColor;\n"
"uniform mat4 mvp;\n"
"uniform mat4 matNormal;\n"
"out vec3 fragNormal;\n"
"out vec4 fragColor;\n"
"void main(){\n"
"    fragNormal = normalize(vec3(matNormal*vec4(vertexNormal,1.0)));\n"
"    fragColor = vertexColor;\n"
"    gl_Position = mvp*vec4(vertexPosition,1.0);\n"
"}\n";
static const char *FS_VC =
"#version 330\n"
"in vec3 fragNormal;\n"
"in vec4 fragColor;\n"
"uniform vec3 lightDir;\n"
"out vec4 finalColor;\n"
"void main(){\n"
"    float d = max(dot(normalize(fragNormal), normalize(-lightDir)), 0.0);\n"
"    float l = 0.42 + 0.58*d;\n"
"    finalColor = vec4(fragColor.rgb*l, 1.0);\n"
"}\n";

/* ── Modèle par type d'ennemi (cadrage PROPRE à chaque type) ──────── */
typedef struct {
    int             have;
    Model           model;
    ModelAnimation *anims;
    int             anim_count;
    int             a_idle, a_walk, a_attack;
    Camera3D        cam;
    float           dst_scale, dst_yanchor;
} EnemyModel;

static int             g_loaded = 0;
static Shader          g_shader_vc;
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

static int find_anim(const EnemyModel *em, const char *name) {
    for (int i = 0; i < em->anim_count; i++)
        if (strcmp(em->anims[i].name, name) == 0) return i;
    return 0;
}

/* Fait tourner `cur` vers `tgt` d'au plus `max_d` rad (chemin le plus court). */
static float angle_approach(float cur, float tgt, float max_d) {
    float d = tgt - cur;
    while (d >  3.14159265f) d -= 6.28318531f;
    while (d < -3.14159265f) d += 6.28318531f;
    if (d >  max_d) d =  max_d;
    if (d < -max_d) d = -max_d;
    return cur + d;
}

static void shade_vc(Model *m) {
    for (int i = 0; i < m->materialCount; i++) {
        m->materials[i].shader = g_shader_vc;
        m->materials[i].maps[MATERIAL_MAP_DIFFUSE].color = WHITE;
    }
}

static void load_enemy(int type, const char *path, Camera3D cam,
                       float dst_scale, float dst_yanchor) {
    EnemyModel *em = &g_em[type];
    em->model = LoadModel(path);
    if (em->model.meshCount == 0) { em->have = 0; return; }
    shade_vc(&em->model);
    em->anims = LoadModelAnimations(path, &em->anim_count);
    em->a_idle   = find_anim(em, "Idle");
    em->a_walk   = find_anim(em, "Walk");
    em->a_attack = find_anim(em, "Attack");
    em->cam = cam; em->dst_scale = dst_scale; em->dst_yanchor = dst_yanchor;
    em->have = 1;
}

void render3d_enemies_init(void) {
    g_loaded = 0;
    for (int i = 0; i < ENEMY_TYPE_COUNT; i++) g_em[i].have = 0;

    g_shader_vc = LoadShaderFromMemory(VS_VC, FS_VC);
    int loc = GetShaderLocation(g_shader_vc, "lightDir");
    Vector3 ld = E_LIGHT;
    if (loc >= 0) SetShaderValue(g_shader_vc, loc, &ld, SHADER_UNIFORM_VEC3);

    /* ── Modèles 3D des ennemis ───────────────────────────────────────
       Aucun modèle propre pour l'instant : les versions définitives iront
       dans assets/3d/3D_enemies/ ; en attendant les ennemis utilisent leur
       SPRITE 2D (les anciennes versions sont archivées dans assets/3d/test/).
       Ajouter un ennemi 3D = exporter son GLB dans 3D_enemies/ + un appel :
         Camera3D cam = { .position={...}, .target={...}, .up={0,1,0},
                          .fovy=..., .projection=CAMERA_ORTHOGRAPHIC };
         load_enemy(ENEMY_XXX, "assets/3d/3D_enemies/xxx.glb", cam, ds, ya); */
    (void)load_enemy;   /* garde le helper prêt (évite -Wunused-function) */

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

void render3d_enemies_prepass(const EnemyPool *ep) {
    if (!g_loaded || ep == NULL) return;
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

        int   fc    = em->anims[aidx].keyframeCount;
        float frame = (fc > 0) ? fmodf(g_anim_t[i] * E_ANIM_FPS, (float)fc) : 0.0f;
        UpdateModelAnimation(em->model, em->anims[aidx], frame);

        ensure_rt(i);
        BeginTextureMode(g_rt[i]);
            ClearBackground(BLANK);
            BeginMode3D(em->cam);
                float yaw = render3d_yaw_for_aim(em->cam, g_facing[i], E_REST_PHI) + E_YAW_OFF;
                rlPushMatrix();
                    rlRotatef(yaw, 0, 1, 0);
                    DrawModel(em->model, (Vector3){0,0,0}, 1.0f, WHITE);
                rlPopMatrix();
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
