/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */
/* ════════════════════════════════════════════════════════════════
   ui/render3d.c — rendu 3D des tours (voir render3d.h).

   Quatre types 3D (interne « kind » : 0 gun, 1 sniper, 2 tesla, 3 flamme) :
   • TOWER_GUN (Mitrailleuse) — base + tourelle + 3 canons (tir séquentiel).
       GLB : tower_base.glb / tower_turret.glb / tower_gun.glb (teinte plate).
   • TOWER_SNIPER — tour de guet HAUTE : statique + mont (azimut) + tube
       (élévation). GLB : tower_sniper_base/mount/barrel.glb, AVEC vertex-colors
       (camo/bois/pierre/métal conservés). Bien plus haut → RT/cam dédiées,
       blit ancré par le bas (le sprite dépasse au-dessus de la case).
   • TOWER_TESLA — base statique vertex-colors + orbe ÉMISSIVE pulsante
       (shader dédié). GLB : tower_tesla_base/orb.glb. Arcs électriques en 2D
       (renderer.c, draw_tesla_arcs), seulement au tir.
   • TOWER_FLAME — lance-flammes : base statique + tourelle ROTATIVE (yaw autour
       de l'axe Y à l'origine = plateau pivotant). GLB : tower_flame_base/turret.glb,
       vertex-colors. Jet de feu dessiné EN 3D dans le RT (draw_flame_jet),
       jaillit du canon UNIQUEMENT au tir.

   La tourelle vise : azimut = tw->angle, élévation = dépression vers l'ennemi
   le plus proche. À chaque tir : recul + flash de bouche (jet pour la flamme).

   Réglages visuels en #define ci-dessous (aucun rendu visible côté outil).
   ════════════════════════════════════════════════════════════════ */
#include "render3d.h"
#include "rlgl.h"
#include <math.h>
#include <stddef.h>

/* ── Réglages TOWER_GUN ──────────────────────────────────────────── */
#define R3D_RT_SIZE     168
#define R3D_ORTHO       4.7f
#define R3D_YAW_OFF     0.0f
#define R3D_YAW_PIVOT_Y 0.92f
#define R3D_GUN_HEIGHT  24.0f
#define R3D_PITCH_MAX   26.0f
#define R3D_RECOIL      0.18f       /* recul d'un canon (un peu plus visible)  */
#define R3D_RECOIL_DUR  0.14f       /* fenêtre recul+flash par balle émise      */
#define R3D_FLASH_DUR   0.05f
static const Vector3 CAM_POS    = { -3.6f, 3.0f, -4.0f };
static const Vector3 CAM_TGT    = {  0.0f, 1.00f, 0.0f };
static const Vector3 GUN_PIVOT  = {  0.0f, 0.557f, -0.58f };
static const Vector3 MUZZLE     = {  0.0f, 0.305f, -1.544f };
static const Vector3 RECOIL_DIR = {  0.0f, -0.105f, 0.995f };
static const float   BARREL_X[3] = { -0.40f, 0.0f, 0.40f };

/* ── Réglages TOWER_SNIPER ───────────────────────────────────────── */
#define SN_RT_W         224
#define SN_RT_H         304
#define SN_ORTHO        8.7f
#define SN_YAW_OFF      0.0f
#define SN_GUN_HEIGHT   40.0f          /* hauteur tube (px) pour l'élévation   */
#define SN_PITCH_MAX    22.0f
#define SN_PITCH_SIGN   (-1.0f)        /* bouche-haut (signe rot Z, à régler)  */
#define SN_RECOIL       0.24f
#define SN_RECOIL_DUR   0.14f
#define SN_FLASH_DUR    0.05f
#define SN_DST_SCALE    2.55f          /* largeur dst = box*SN_DST_SCALE        */
#define SN_DST_YANCHOR  0.84f          /* part de la hauteur dst sous le bas case*/
static const Vector3 SN_CAM_POS     = { -6.0f, 8.2f, -7.0f };
static const Vector3 SN_CAM_TGT     = {  0.0f, 3.25f, 0.0f };
static const Vector3 SN_YAW_PIVOT   = {  0.32f, 5.06f, 0.0f };  /* Y-up           */
static const Vector3 SN_PITCH_PIVOT = {  0.51f, 0.46f, 0.0f };  /* rel. yaw, Y-up */
static const Vector3 SN_MUZZLE      = {  1.83f, 0.02f, 0.0f };  /* rel. pitch     */
static const Vector3 SN_RECOIL_DIR  = { -1.0f, 0.0f, 0.0f };

/* ── Réglages TOWER_TESLA ────────────────────────────────────────── */
#define TE_RT_W         200
#define TE_RT_H         248
#define TE_ORTHO        4.9f
#define TE_DST_SCALE    1.70f          /* largeur dst = box*TE_DST_SCALE        */
#define TE_DST_YANCHOR  0.98f          /* base ≈ bas de la case (grounded)      */
#define TE_ORB_PULSE    0.05f          /* amplitude de pulsation de l'orbe      */
static const Vector3 TE_CAM_POS = { -4.2f, 4.7f, -5.2f };
static const Vector3 TE_CAM_TGT = {  0.0f, 2.40f, 0.0f };  /* base ~ bas du RT  */
static const Vector3 TE_ORB_POS = {  0.0f, 3.00f, 0.0f };  /* centre orbe (Y-up) */

/* ── Réglages TOWER_FLAME ────────────────────────────────────────── */
#define FL_RT_W         236
#define FL_RT_H         236
#define FL_ORTHO        6.2f            /* large : doit contenir le balayage du  */
                                        /* canon + le jet dans toute direction   */
#define FL_YAW_OFF      0.0f            /* canon +X comme le sniper (offset 0)   */
#define FL_DST_SCALE    2.50f           /* largeur dst = box*FL_DST_SCALE        */
#define FL_DST_YANCHOR  0.78f           /* base ≈ bas de la case (grounded)      */
/* Fenêtre du jet : DOIT dépasser la période de tir (fire_rate=3.0 → 0.333 s)
   sinon le jet clignote entre deux tirs. Réarmée à chaque tir → jet CONTINU
   tant que la tour tire, qui ne s'éteint que ~FL_FIRE_DUR après le dernier. */
#define FL_FIRE_DUR     0.42f
static const Vector3 FL_CAM_POS  = { -4.6f, 4.5f, -5.5f };
static const Vector3 FL_CAM_TGT  = {  0.0f, 1.05f, 0.0f };  /* axe → balayage centré */
/* Bouche du canon + axe de tir (Y-up, repère tourelle local, yaw=0).
   Bouche monde Z-up=(1.692,0.169,1.47) → Y-up=(x,z,-y). Axe canon Z-up
   d=(cos6,0,sin6) → Y-up=(cos6,sin6,0). */
static const Vector3 FL_MUZZLE   = {  1.692f, 1.470f, -0.169f };
static const Vector3 FL_FIRE_DIR = {  0.9945f, 0.1045f, 0.0f };

static const Vector3 LIGHT_DIR  = { -0.45f, -0.80f, -0.40f };

/* Orientation au repos (angle sol raylib) de l'AVANT de chaque modèle, pour le
   helper de visée render3d_yaw_for_aim : la mitrailleuse pointe -Z (π), le
   sniper et la flamme pointent +X (π/2). Si la visée est décalée d'un angle
   CONSTANT en jeu, ajuster le *_YAW_OFF correspondant (offset en degrés). */
#define R3D_REST_PHI  3.14159265f
#define SN_REST_PHI   1.57079633f
#define FL_REST_PHI   1.57079633f

/* ── Shader éclairage directionnel — teinte plate (TOWER_GUN) ────── */
static const char *VS =
"#version 330\n"
"in vec3 vertexPosition;\n"
"in vec2 vertexTexCoord;\n"
"in vec3 vertexNormal;\n"
"uniform mat4 mvp;\n"
"uniform mat4 matNormal;\n"
"out vec2 fragTexCoord;\n"
"out vec3 fragNormal;\n"
"void main(){\n"
"    fragTexCoord = vertexTexCoord;\n"
"    fragNormal = normalize(vec3(matNormal*vec4(vertexNormal,1.0)));\n"
"    gl_Position = mvp*vec4(vertexPosition,1.0);\n"
"}\n";
static const char *FS =
"#version 330\n"
"in vec2 fragTexCoord;\n"
"in vec3 fragNormal;\n"
"uniform sampler2D texture0;\n"
"uniform vec4 colDiffuse;\n"
"uniform vec3 lightDir;\n"
"out vec4 finalColor;\n"
"void main(){\n"
"    vec4 base = texture(texture0, fragTexCoord)*colDiffuse;\n"
"    float d = max(dot(normalize(fragNormal), normalize(-lightDir)), 0.0);\n"
"    float l = 0.38 + 0.62*d;\n"
"    finalColor = vec4(base.rgb*l, 1.0);\n"
"}\n";

/* ── Shader VERTEX-COLOR (TOWER_SNIPER) ──────────────────────────── */
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

/* ── Shader ÉMISSIF (orbe Tesla : plasma violet brillant, pulsé) ─── */
static const char *VS_EMIT =
"#version 330\n"
"in vec3 vertexPosition;\n"
"in vec4 vertexColor;\n"
"uniform mat4 mvp;\n"
"out vec4 fragColor;\n"
"void main(){\n"
"    fragColor = vertexColor;\n"
"    gl_Position = mvp*vec4(vertexPosition,1.0);\n"
"}\n";
static const char *FS_EMIT =
"#version 330\n"
"in vec4 fragColor;\n"
"uniform float glow;\n"
"out vec4 finalColor;\n"
"void main(){\n"
"    finalColor = vec4(fragColor.rgb*glow + vec3(0.10,0.03,0.16), 1.0);\n"
"}\n";

/* ── État ───────────────────────────────────────────────────────── */
static int             g_loaded     = 0;
static int             g_have_gun   = 0;
static int             g_have_snipr = 0;
static int             g_have_tesla = 0;
static int             g_have_flame = 0;
static Model           g_base, g_turret, g_gun;
static Model           g_sn_base, g_sn_mount, g_sn_barrel;
static Model           g_te_base, g_te_orb;
static Model           g_fl_base, g_fl_turret;
static Shader          g_shader, g_shader_vc, g_shader_emit;
static Camera3D        g_cam, g_cam_sn, g_cam_te, g_cam_fl;
static RenderTexture2D g_rt[MAX_TOWERS];
static int             g_rt_kind[MAX_TOWERS];   /* -1 aucun, 0 gun, 1 sniper, 2 tesla, 3 flamme */
static int             g_used[MAX_TOWERS];
static float           g_prev_ft[MAX_TOWERS];
static float           g_fire_t [MAX_TOWERS];
static int             g_barrel [MAX_TOWERS];   /* canon courant (Mitrailleuse) */

static void tint_model(Model *m, Color c) {
    for (int i = 0; i < m->materialCount; i++) {
        m->materials[i].shader = g_shader;
        m->materials[i].maps[MATERIAL_MAP_DIFFUSE].color = c;
    }
}
static void shade_vc(Model *m) {
    for (int i = 0; i < m->materialCount; i++) {
        m->materials[i].shader = g_shader_vc;
        m->materials[i].maps[MATERIAL_MAP_DIFFUSE].color = WHITE;
    }
}
static void shade_emit(Model *m) {
    for (int i = 0; i < m->materialCount; i++)
        m->materials[i].shader = g_shader_emit;
}
static void set_light(Shader sh) {
    int loc = GetShaderLocation(sh, "lightDir");
    Vector3 ld = LIGHT_DIR;
    if (loc >= 0) SetShaderValue(sh, loc, &ld, SHADER_UNIFORM_VEC3);
}

void render3d_init(void) {
    g_loaded = 0; g_have_gun = 0; g_have_snipr = 0; g_have_tesla = 0; g_have_flame = 0;

    g_shader      = LoadShaderFromMemory(VS, FS);
    g_shader_vc   = LoadShaderFromMemory(VS_VC, FS_VC);
    g_shader_emit = LoadShaderFromMemory(VS_EMIT, FS_EMIT);
    set_light(g_shader); set_light(g_shader_vc);

    /* TOWER_GUN */
    g_base   = LoadModel("assets/3d/3D_Tours/tower_base.glb");
    g_turret = LoadModel("assets/3d/3D_Tours/tower_turret.glb");
    g_gun    = LoadModel("assets/3d/3D_Tours/tower_gun.glb");
    if (g_base.meshCount && g_turret.meshCount && g_gun.meshCount) {
        tint_model(&g_base,   (Color){150, 72, 42, 255});
        tint_model(&g_turret, (Color){ 92, 90, 98, 255});
        tint_model(&g_gun,    (Color){ 62, 60, 66, 255});
        g_have_gun = 1;
    }

    /* TOWER_SNIPER (vertex-colors) */
    g_sn_base   = LoadModel("assets/3d/3D_Tours/tower_sniper_base.glb");
    g_sn_mount  = LoadModel("assets/3d/3D_Tours/tower_sniper_mount.glb");
    g_sn_barrel = LoadModel("assets/3d/3D_Tours/tower_sniper_barrel.glb");
    if (g_sn_base.meshCount && g_sn_mount.meshCount && g_sn_barrel.meshCount) {
        shade_vc(&g_sn_base); shade_vc(&g_sn_mount); shade_vc(&g_sn_barrel);
        g_have_snipr = 1;
    }

    /* TOWER_TESLA (base vertex-colors + orbe émissive) */
    g_te_base = LoadModel("assets/3d/3D_Tours/tower_tesla_base.glb");
    g_te_orb  = LoadModel("assets/3d/3D_Tours/tower_tesla_orb.glb");
    if (g_te_base.meshCount && g_te_orb.meshCount) {
        shade_vc(&g_te_base); shade_emit(&g_te_orb);
        g_have_tesla = 1;
    }

    /* TOWER_FLAME (base statique + tourelle rotative, vertex-colors).
       Le jet de feu est dessiné en 3D dans le RT (cf. draw_flame_jet). */
    g_fl_base   = LoadModel("assets/3d/3D_Tours/tower_flame_base.glb");
    g_fl_turret = LoadModel("assets/3d/3D_Tours/tower_flame_turret.glb");
    if (g_fl_base.meshCount && g_fl_turret.meshCount) {
        shade_vc(&g_fl_base); shade_vc(&g_fl_turret);
        g_have_flame = 1;
    }

    g_cam.position   = CAM_POS;   g_cam.target     = CAM_TGT;
    g_cam.up         = (Vector3){0,1,0};
    g_cam.fovy       = R3D_ORTHO; g_cam.projection = CAMERA_ORTHOGRAPHIC;

    g_cam_sn.position   = SN_CAM_POS; g_cam_sn.target     = SN_CAM_TGT;
    g_cam_sn.up         = (Vector3){0,1,0};
    g_cam_sn.fovy       = SN_ORTHO;   g_cam_sn.projection = CAMERA_ORTHOGRAPHIC;

    g_cam_te.position   = TE_CAM_POS; g_cam_te.target     = TE_CAM_TGT;
    g_cam_te.up         = (Vector3){0,1,0};
    g_cam_te.fovy       = TE_ORTHO;   g_cam_te.projection = CAMERA_ORTHOGRAPHIC;

    g_cam_fl.position   = FL_CAM_POS; g_cam_fl.target     = FL_CAM_TGT;
    g_cam_fl.up         = (Vector3){0,1,0};
    g_cam_fl.fovy       = FL_ORTHO;   g_cam_fl.projection = CAMERA_ORTHOGRAPHIC;

    for (int i = 0; i < MAX_TOWERS; i++) {
        g_rt[i].id   = 0;  g_rt_kind[i] = -1;  g_used[i] = 0;
        g_prev_ft[i] = 0;  g_fire_t[i]  = 0;   g_barrel[i] = 0;
    }
    g_loaded = (g_have_gun || g_have_snipr || g_have_tesla || g_have_flame);
}

void render3d_shutdown(void) {
    if (!g_loaded) return;
    for (int i = 0; i < MAX_TOWERS; i++)
        if (g_rt[i].id != 0) UnloadRenderTexture(g_rt[i]);
    UnloadShader(g_shader); UnloadShader(g_shader_vc); UnloadShader(g_shader_emit);
    if (g_have_gun)   { UnloadModel(g_base);    UnloadModel(g_turret);  UnloadModel(g_gun); }
    if (g_have_snipr) { UnloadModel(g_sn_base); UnloadModel(g_sn_mount); UnloadModel(g_sn_barrel); }
    if (g_have_tesla) { UnloadModel(g_te_base); UnloadModel(g_te_orb); }
    if (g_have_flame) { UnloadModel(g_fl_base); UnloadModel(g_fl_turret); }
    g_loaded = 0;
}

int render3d_available(void) { return g_loaded; }

/* (Ré)alloue le RT du slot i pour le type voulu (carré=gun, haut=sniper). */
static void ensure_rt(int i, int kind) {
    if (g_rt_kind[i] == kind && g_rt[i].id != 0) return;
    if (g_rt[i].id != 0) UnloadRenderTexture(g_rt[i]);
    int w = (kind == 1) ? SN_RT_W : (kind == 2) ? TE_RT_W : (kind == 3) ? FL_RT_W : R3D_RT_SIZE;
    int h = (kind == 1) ? SN_RT_H : (kind == 2) ? TE_RT_H : (kind == 3) ? FL_RT_H : R3D_RT_SIZE;
    g_rt[i] = LoadRenderTexture(w, h);
    g_rt_kind[i] = kind;
}

/* Dépression vers l'ennemi le plus proche (canon au-dessus du sol). */
static float pitch_to_enemy(const Tower *tw, const EnemyPool *ep,
                            float gun_h, float pmax) {
    if (!ep) return 0.0f;
    float bd2 = 1e18f, best = -1.0f;
    for (int i = 0; i < MAX_ENEMIES; i++) {
        const Enemy *e = &ep->enemies[i];
        if (!e->active) continue;
        float dx = e->x - tw->cx, dy = e->y - tw->cy;
        float d2 = dx*dx + dy*dy;
        if (d2 < bd2) { bd2 = d2; best = sqrtf(d2); }
    }
    if (best < 0.0f) return 0.0f;
    if (best < 1.0f) best = 1.0f;
    float p = atan2f(gun_h, best) * RAD2DEG;
    return (p > pmax) ? pmax : p;
}

/* ── TOWER_GUN : base + tourelle (azimut) + 3 canons (un par balle) ────
   Le canon `fire_barrel` (avancé à CHAQUE balle émise, cf. prepass) recule +
   flash de bouche pendant la fenêtre de tir. fire_t décroît de R3D_RECOIL_DUR
   à 0 → recul et flash s'estompent. Chaque canon tire donc à son tour, en
   synchro avec l'émission des projectiles. */
static void draw_tower_model(float yaw_deg, float pitch_deg, int fire_barrel, float fire_t) {
    DrawModel(g_base, (Vector3){0,0,0}, 1.0f, WHITE);
    rlPushMatrix();
        rlTranslatef(0.0f, R3D_YAW_PIVOT_Y, 0.0f);
        rlRotatef(yaw_deg, 0, 1, 0);
        DrawModel(g_turret, (Vector3){0,0,0}, 1.0f, WHITE);
        rlTranslatef(GUN_PIVOT.x, GUN_PIVOT.y, GUN_PIVOT.z);
        rlRotatef(pitch_deg, 1, 0, 0);

        float fa = (fire_t > 0.0f) ? fire_t / R3D_RECOIL_DUR : 0.0f;   /* 1 → 0 */
        if (fa > 1.0f) fa = 1.0f;
        for (int b = 0; b < 3; b++) {
            float r = (b == fire_barrel) ? R3D_RECOIL * fa : 0.0f;
            rlPushMatrix();
                rlTranslatef(BARREL_X[b] + RECOIL_DIR.x*r, RECOIL_DIR.y*r, RECOIL_DIR.z*r);
                DrawModel(g_gun, (Vector3){0,0,0}, 1.0f, WHITE);
            rlPopMatrix();
        }
        if (fa > 0.0f) {     /* flash sur TOUTE la fenêtre, taille décroissante */
            Vector3 m = { BARREL_X[fire_barrel] + MUZZLE.x, MUZZLE.y, MUZZLE.z };
            DrawSphere(m, 0.15f + 0.13f*fa, (Color){255, 224, 130, 255});
            DrawSphere(m, 0.08f + 0.07f*fa, (Color){255, 255, 220, 255});
        }
    rlPopMatrix();
}

/* ── TOWER_SNIPER : statique + mont (azimut) + tube (élévation+recul) ─ */
static void draw_sniper_model(float yaw_deg, float pitch_deg, float fire_t) {
    DrawModel(g_sn_base, (Vector3){0,0,0}, 1.0f, WHITE);
    rlPushMatrix();
        rlTranslatef(SN_YAW_PIVOT.x, SN_YAW_PIVOT.y, SN_YAW_PIVOT.z);
        rlRotatef(yaw_deg, 0, 1, 0);
        DrawModel(g_sn_mount, (Vector3){0,0,0}, 1.0f, WHITE);
        rlTranslatef(SN_PITCH_PIVOT.x, SN_PITCH_PIVOT.y, SN_PITCH_PIVOT.z);
        rlRotatef(pitch_deg, 0, 0, 1);
        float r = (fire_t > 0.0f) ? SN_RECOIL * (fire_t / SN_RECOIL_DUR) : 0.0f;
        rlPushMatrix();
            rlTranslatef(SN_RECOIL_DIR.x*r, SN_RECOIL_DIR.y*r, SN_RECOIL_DIR.z*r);
            DrawModel(g_sn_barrel, (Vector3){0,0,0}, 1.0f, WHITE);
        rlPopMatrix();
        if (fire_t > (SN_RECOIL_DUR - SN_FLASH_DUR)) {
            Vector3 m = { SN_MUZZLE.x + SN_RECOIL_DIR.x*r, SN_MUZZLE.y, SN_MUZZLE.z };
            DrawSphere(m, 0.34f, (Color){255, 224, 130, 255});
            DrawSphere(m, 0.18f, (Color){255, 255, 220, 255});
        }
    rlPopMatrix();
}

/* ── TOWER_TESLA : modèle statique + orbe émissive pulsante ─────── */
static void draw_tesla_model(float t) {
    DrawModel(g_te_base, (Vector3){0,0,0}, 1.0f, WHITE);
    float pulse = 1.0f + TE_ORB_PULSE * sinf(t * 4.5f);
    float glow  = 1.45f + 0.25f * sinf(t * 4.5f);
    int loc = GetShaderLocation(g_shader_emit, "glow");
    if (loc >= 0) SetShaderValue(g_shader_emit, loc, &glow, SHADER_UNIFORM_FLOAT);
    rlPushMatrix();
        rlTranslatef(TE_ORB_POS.x, TE_ORB_POS.y, TE_ORB_POS.z);
        DrawModel(g_te_orb, (Vector3){0,0,0}, pulse, WHITE);
    rlPopMatrix();
}

/* ── TOWER_FLAME : jet de feu jaillissant du canon (UNIQUEMENT au tir) ─
   Dessiné dans le repère tourelle (après le yaw) → le jet suit la bouche.
   Cônes emboîtés du plus long/sombre (langue rouge) au plus court/clair
   (cœur), + éclat de bouche. Longueur et éclat scintillent. */
static void draw_flame_jet(float fire_t, float t) {
    float inten = fire_t / FL_FIRE_DUR;            /* 1 au tir → 0          */
    if (inten < 0.0f) inten = 0.0f;
    if (inten > 1.0f) inten = 1.0f;
    float fl  = 0.78f + 0.25f * sinf(t * 27.0f) + 0.12f * sinf(t * 61.0f);  /* flicker */
    if (fl < 0.50f) fl = 0.50f;
    if (fl > 1.15f) fl = 1.15f;
    float len = (0.80f + 0.45f * inten) * fl;      /* portée du jet (bornée) */
    Vector3 m = FL_MUZZLE, d = FL_FIRE_DIR;
    float   wob = 0.10f * sinf(t * 18.0f);         /* fouettement latéral   */
    Vector3 tip = { m.x + d.x*len,         m.y + d.y*len + wob*0.5f, m.z + d.z*len + wob };
    Vector3 pm  = { m.x + d.x*len*0.60f,   m.y + d.y*len*0.60f,      m.z + d.z*len*0.60f };
    Vector3 pc  = { m.x + d.x*len*0.28f,   m.y + d.y*len*0.28f,      m.z + d.z*len*0.28f };
    unsigned char a = (unsigned char)(185 + 70*inten);
    DrawCylinderEx(m, tip, 0.20f*fl, 0.05f, 9, (Color){208,  55, 18, a});  /* langue rouge */
    DrawCylinderEx(m, pm,  0.27f*fl, 0.10f, 9, (Color){255, 120, 28, a});  /* corps orange */
    DrawCylinderEx(m, pc,  0.22f*fl, 0.07f, 9, (Color){255, 238,150, a});  /* cœur clair   */
    DrawSphere(m, 0.16f*fl, (Color){255, 244, 190, a});                    /* éclat bouche */
}

/* ── TOWER_FLAME : base statique + tourelle rotative (yaw) + jet ─────── */
static void draw_flame_model(float yaw_deg, float fire_t, float t) {
    DrawModel(g_fl_base, (Vector3){0,0,0}, 1.0f, WHITE);
    rlPushMatrix();
        rlRotatef(yaw_deg, 0, 1, 0);          /* plateau rotatif (axe Y, origine) */
        DrawModel(g_fl_turret, (Vector3){0,0,0}, 1.0f, WHITE);
        if (fire_t > 0.0f) draw_flame_jet(fire_t, t);
    rlPopMatrix();
}

void render3d_prepass(const TowerPool *tp, const EnemyPool *ep) {
    if (!g_loaded || tp == NULL) return;
    float dt = GetFrameTime();
    for (int i = 0; i < MAX_TOWERS; i++) {
        const Tower *tw = &tp->towers[i];
        int kind = -1;
        if (tw->active) {
            if (tw->type == TOWER_GUN    && g_have_gun)   kind = 0;
            if (tw->type == TOWER_SNIPER && g_have_snipr) kind = 1;
            if (tw->type == TOWER_TESLA  && g_have_tesla) kind = 2;
            if (tw->type == TOWER_FLAME  && g_have_flame) kind = 3;
        }
        if (kind < 0) { g_used[i] = 0; g_fire_t[i] = 0.0f; continue; }

        /* Détection du tir : fire_timer remonte = UNE balle émise. À chaque
           balle, la Mitrailleuse passe au canon SUIVANT (tir en cascade). */
        float dur = (kind == 1) ? SN_RECOIL_DUR
                  : (kind == 3) ? FL_FIRE_DUR : R3D_RECOIL_DUR;
        if (tw->fire_timer > g_prev_ft[i] + 0.02f) {
            g_barrel[i] = (g_barrel[i] + 1) % 3;   /* canon suivant par balle */
            g_fire_t[i] = dur;
        }
        g_prev_ft[i] = tw->fire_timer;
        g_fire_t[i] -= dt;
        if (g_fire_t[i] < 0.0f) g_fire_t[i] = 0.0f;

        /* Yaw CORRECT pour la caméra oblique (cf. render3d_yaw_for_aim) : on
           projette la visée tw->angle via la caméra du type. Chaque type a sa
           caméra et son orientation au repos. */
        float yaw;
        if      (kind == 1) yaw = render3d_yaw_for_aim(g_cam_sn, tw->angle, SN_REST_PHI) + SN_YAW_OFF;
        else if (kind == 3) yaw = render3d_yaw_for_aim(g_cam_fl, tw->angle, FL_REST_PHI) + FL_YAW_OFF;
        else                yaw = render3d_yaw_for_aim(g_cam,    tw->angle, R3D_REST_PHI) + R3D_YAW_OFF;

        ensure_rt(i, kind);
        BeginTextureMode(g_rt[i]);
            ClearBackground(BLANK);
            if (kind == 3) {
                BeginMode3D(g_cam_fl);
                    draw_flame_model(yaw, g_fire_t[i], (float)GetTime());
                EndMode3D();
            } else if (kind == 2) {
                BeginMode3D(g_cam_te);
                    draw_tesla_model((float)GetTime());
                EndMode3D();
            } else if (kind == 1) {
                BeginMode3D(g_cam_sn);
                    float pitch = SN_PITCH_SIGN * pitch_to_enemy(tw, ep, SN_GUN_HEIGHT, SN_PITCH_MAX);
                    draw_sniper_model(yaw, pitch, g_fire_t[i]);
                EndMode3D();
            } else {
                BeginMode3D(g_cam);
                    float pitch = pitch_to_enemy(tw, ep, R3D_GUN_HEIGHT, R3D_PITCH_MAX);
                    draw_tower_model(yaw, pitch, g_barrel[i], g_fire_t[i]);
                EndMode3D();
            }
        EndTextureMode();
        g_used[i] = 1;
    }
}

Texture2D render3d_tower_tex(int tower_index) {
    if (!g_loaded || tower_index < 0 || tower_index >= MAX_TOWERS || !g_used[tower_index])
        return (Texture2D){0};
    return g_rt[tower_index].texture;
}

Rectangle render3d_tower_dst(int tower_index, float cx, float cy, float tile) {
    float box = tile - 8.0f;
    int kind = (tower_index >= 0 && tower_index < MAX_TOWERS) ? g_rt_kind[tower_index] : -1;
    if (kind == 1) {
        /* Sniper : haut et ancré par le bas (dépasse au-dessus de la case). */
        float w = box * SN_DST_SCALE;
        float h = w * ((float)SN_RT_H / (float)SN_RT_W);
        return (Rectangle){ cx - w*0.5f, cy + box*0.5f - h*SN_DST_YANCHOR, w, h };
    }
    if (kind == 2) {
        /* Tesla : stout, ancré par le bas (l'orbe dépasse un peu). */
        float w = box * TE_DST_SCALE;
        float h = w * ((float)TE_RT_H / (float)TE_RT_W);
        return (Rectangle){ cx - w*0.5f, cy + box*0.5f - h*TE_DST_YANCHOR, w, h };
    }
    if (kind == 3) {
        /* Flamme : large (canon), ancré par le bas. */
        float w = box * FL_DST_SCALE;
        float h = w * ((float)FL_RT_H / (float)FL_RT_W);
        return (Rectangle){ cx - w*0.5f, cy + box*0.5f - h*FL_DST_YANCHOR, w, h };
    }
    return (Rectangle){ cx - box*0.5f, cy - box*0.5f, box, box };
}

/* ════════════════════════════════════════════════════
   MODE HÉROS — dessin direct d'une tour dans la scène 3D
   (socle fixe + tourelle orientée vers map_angle)
   ════════════════════════════════════════════════════ */
/* Décalage constant du cap de tourelle en monde (réglage playtest). */
#define R3D_TURRET_WORLD_YAW_OFF  0.0f

int render3d_tower_draw_world(int type, Vector3 pos, float map_angle,
                              float scale) {
    if (!g_loaded) return 0;
    /* Direction sim (cos a, sin a) → cap monde atan2(x, z). */
    float yaw_deg = atan2f(cosf(map_angle), sinf(map_angle)) * RAD2DEG
                  + R3D_TURRET_WORLD_YAW_OFF;
    int drawn = 1;

    rlPushMatrix();
    rlTranslatef(pos.x, pos.y, pos.z);
    rlScalef(scale, scale, scale);
    switch (type) {
        case TOWER_GUN:
            if (!g_have_gun) { drawn = 0; break; }
            DrawModel(g_base, (Vector3){0, 0, 0}, 1.0f, WHITE);
            rlRotatef(yaw_deg, 0.0f, 1.0f, 0.0f);
            DrawModel(g_turret, (Vector3){0, 0, 0}, 1.0f, WHITE);
            DrawModel(g_gun,    (Vector3){0, 0, 0}, 1.0f, WHITE);
            break;
        case TOWER_SNIPER:
            if (!g_have_snipr) { drawn = 0; break; }
            DrawModel(g_sn_base, (Vector3){0, 0, 0}, 1.0f, WHITE);
            rlRotatef(yaw_deg, 0.0f, 1.0f, 0.0f);
            DrawModel(g_sn_mount,  (Vector3){0, 0, 0}, 1.0f, WHITE);
            DrawModel(g_sn_barrel, (Vector3){0, 0, 0}, 1.0f, WHITE);
            break;
        case TOWER_TESLA:
            if (!g_have_tesla) { drawn = 0; break; }
            DrawModel(g_te_base, (Vector3){0, 0, 0}, 1.0f, WHITE);
            /* Orbe : flottement lent (pas d'orientation nécessaire). */
            rlTranslatef(0.0f, sinf((float)GetTime() * 2.2f) * 0.06f, 0.0f);
            DrawModel(g_te_orb, (Vector3){0, 0, 0}, 1.0f, WHITE);
            break;
        case TOWER_FLAME:
            if (!g_have_flame) { drawn = 0; break; }
            DrawModel(g_fl_base, (Vector3){0, 0, 0}, 1.0f, WHITE);
            rlRotatef(yaw_deg, 0.0f, 1.0f, 0.0f);
            DrawModel(g_fl_turret, (Vector3){0, 0, 0}, 1.0f, WHITE);
            break;
        default:
            drawn = 0;
            break;
    }
    rlPopMatrix();
    return drawn;
}
