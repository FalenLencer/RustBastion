/* ════════════════════════════════════════════════════════════════
   audio.c — RUST BASTION
   Corrections v2 :
   - Throttle SFX haute fréquence (tours) → pas de saturation audio
   - Tableau g_sfx_valid[] / g_music_valid[] → guard avant tout call
   - audio_apply_volumes() n'itère que les slots valides
   - audio_update() : UpdateMusicStream d'abord, relance si arrêté
   - Volumes par défaut cohérents avec le menu
   ════════════════════════════════════════════════════════════════ */
#include "audio.h"
#include "raylib.h"
#include <string.h>
#include <stdio.h>

/* ── Chemins des assets ──────────────────────────────────────── */
static const char *const AUDIO_MUSIC_FILES[AUDIO_MUSIC_COUNT] = {
    "assets/sounds/music/ambience_wasteland.wav",
    "assets/sounds/music/ambience_swamp.wav",
    "assets/sounds/music/ambience_desert.wav",
    "assets/sounds/music/ambience_city.wav",
    "assets/sounds/music/ambience_factory.wav",
};

static const char *const AUDIO_SFX_FILES[AUDIO_SFX_COUNT] = {
    "assets/sounds/sfx/menu_click.wav",
    "assets/sounds/sfx/menu_confirm.wav",
    "assets/sounds/sfx/tower_place.wav",
    "assets/sounds/sfx/tower_fire_gun.wav",
    "assets/sounds/sfx/tower_fire_sniper.wav",
    "assets/sounds/sfx/tower_fire_flame.wav",
    "assets/sounds/sfx/tower_fire_tesla.wav",
    "assets/sounds/sfx/unit_spawn.wav",
    "assets/sounds/sfx/enemy_spawn.wav",
    "assets/sounds/sfx/enemy_hit.wav",
    "assets/sounds/sfx/enemy_death.wav",
    "assets/sounds/sfx/wave_start.wav",
    "assets/sounds/sfx/game_over.wav",
    "assets/sounds/sfx/victory.wav",
    "assets/sounds/sfx/material_collect.wav",
    "assets/sounds/sfx/material_apply.wav",
};

/* ── État interne ────────────────────────────────────────────── */
static Music  g_music[AUDIO_MUSIC_COUNT];
static Sound  g_sfx[AUDIO_SFX_COUNT];
static int    g_sfx_valid[AUDIO_SFX_COUNT];
static int    g_music_valid[AUDIO_MUSIC_COUNT];
static int    g_audio_ready   = 0;
static int    g_current_music = AUDIO_MUSIC_NONE;
static float  g_master_volume = 0.50f;
static float  g_music_volume  = 0.50f;
static float  g_sfx_volume    = 0.50f;

/* ── Throttle SFX haute fréquence ────────────────────────────── */
#define THROTTLE_COUNT 4
static const AudioSfxID THROTTLED_SFX[THROTTLE_COUNT] = {
    AUDIO_SFX_TOWER_FIRE_GUN,
    AUDIO_SFX_TOWER_FIRE_SNIPER,
    AUDIO_SFX_TOWER_FIRE_FLAME,
    AUDIO_SFX_TOWER_FIRE_TESLA,
};
static const float THROTTLE_DELAY[THROTTLE_COUNT] = {
    0.08f,   /* gun    */
    0.12f,   /* sniper */
    0.06f,   /* flame  */
    0.10f,   /* tesla  */
};
static float g_throttle_timer[AUDIO_SFX_COUNT];

/* ════════════════════════════════════════════════════════════════
   INTERNES
   ════════════════════════════════════════════════════════════════ */
static float clamp01(float v) {
    return v < 0.0f ? 0.0f : v > 1.0f ? 1.0f : v;
}

static void audio_apply_volumes(void) {
    if (!g_audio_ready) return;
    float mv = g_music_volume * g_master_volume;
    float sv = g_sfx_volume   * g_master_volume;
    for (int i = 0; i < AUDIO_MUSIC_COUNT; i++)
        if (g_music_valid[i]) SetMusicVolume(g_music[i], mv);
    for (int i = 0; i < AUDIO_SFX_COUNT; i++)
        if (g_sfx_valid[i])   SetSoundVolume(g_sfx[i], sv);
}

/* ════════════════════════════════════════════════════════════════
   INIT / SHUTDOWN
   ════════════════════════════════════════════════════════════════ */
int audio_init(void) {
    if (g_audio_ready) return 1;

    memset(g_sfx_valid,      0, sizeof(g_sfx_valid));
    memset(g_music_valid,    0, sizeof(g_music_valid));
    memset(g_throttle_timer, 0, sizeof(g_throttle_timer));

    InitAudioDevice();
    if (!IsAudioDeviceReady()) {
        fprintf(stderr, "[AUDIO] Échec init device\n");
        return 0;
    }
    g_audio_ready = 1;

    for (int i = 0; i < AUDIO_SFX_COUNT; i++) {
        g_sfx[i]       = LoadSound(AUDIO_SFX_FILES[i]);
        g_sfx_valid[i] = IsSoundValid(g_sfx[i]) ? 1 : 0;
        if (!g_sfx_valid[i])
            fprintf(stderr, "[AUDIO] SFX manquant : %s\n", AUDIO_SFX_FILES[i]);
    }

    for (int i = 0; i < AUDIO_MUSIC_COUNT; i++) {
        g_music[i]       = LoadMusicStream(AUDIO_MUSIC_FILES[i]);
        g_music_valid[i] = IsMusicValid(g_music[i]) ? 1 : 0;
        if (g_music_valid[i])
            g_music[i].looping = true;
        else
            fprintf(stderr, "[AUDIO] Musique manquante : %s\n",
                    AUDIO_MUSIC_FILES[i]);
    }

    audio_apply_volumes();
    fprintf(stderr, "[AUDIO] Prêt\n");
    return 1;
}

void audio_shutdown(void) {
    if (!g_audio_ready) return;
    audio_stop_music();
    for (int i = 0; i < AUDIO_SFX_COUNT; i++)
        if (g_sfx_valid[i])   UnloadSound(g_sfx[i]);
    for (int i = 0; i < AUDIO_MUSIC_COUNT; i++)
        if (g_music_valid[i]) UnloadMusicStream(g_music[i]);
    CloseAudioDevice();
    g_audio_ready   = 0;
    g_current_music = AUDIO_MUSIC_NONE;
}

/* ════════════════════════════════════════════════════════════════
   UPDATE — appelé chaque frame depuis main.c
   ════════════════════════════════════════════════════════════════ */
void audio_update(void) {
    if (!g_audio_ready) return;

    /* Décrémente les timers de throttle */
    float dt = GetFrameTime();
    for (int i = 0; i < AUDIO_SFX_COUNT; i++) {
        if (g_throttle_timer[i] > 0.0f) {
            g_throttle_timer[i] -= dt;
            if (g_throttle_timer[i] < 0.0f) g_throttle_timer[i] = 0.0f;
        }
    }

    /* Stream musique */
    if (g_current_music == AUDIO_MUSIC_NONE) return;
    if (!g_music_valid[g_current_music])     return;
    UpdateMusicStream(g_music[g_current_music]);
    /* Relance si stoppé (fin de fichier non-loopé, reprise OS) */
    if (!IsMusicStreamPlaying(g_music[g_current_music]))
        PlayMusicStream(g_music[g_current_music]);
}

/* ════════════════════════════════════════════════════════════════
   SFX
   ════════════════════════════════════════════════════════════════ */
void audio_play_sfx(AudioSfxID id) {
    if (!g_audio_ready) return;
    if (id < 0 || id >= AUDIO_SFX_COUNT) return;
    if (!g_sfx_valid[id]) return;
    if (g_throttle_timer[id] > 0.0f) return;

    PlaySound(g_sfx[id]);

    for (int k = 0; k < THROTTLE_COUNT; k++) {
        if (THROTTLED_SFX[k] == id) {
            g_throttle_timer[id] = THROTTLE_DELAY[k];
            break;
        }
    }
}

/* ════════════════════════════════════════════════════════════════
   MUSIQUE
   ════════════════════════════════════════════════════════════════ */
void audio_play_theme_music(ThemeID theme) {
    if (!g_audio_ready) return;
    int next = (int)theme;
    if (next < 0 || next >= AUDIO_MUSIC_COUNT) next = AUDIO_MUSIC_WASTELAND;

    if (g_current_music == next &&
        g_music_valid[next] &&
        IsMusicStreamPlaying(g_music[next])) return;

    if (g_current_music != AUDIO_MUSIC_NONE &&
        g_music_valid[g_current_music] &&
        IsMusicStreamPlaying(g_music[g_current_music]))
        StopMusicStream(g_music[g_current_music]);

    g_current_music = next;
    if (!g_music_valid[g_current_music]) return;

    SetMusicVolume(g_music[g_current_music],
                   g_music_volume * g_master_volume);
    PlayMusicStream(g_music[g_current_music]);
}

void audio_stop_music(void) {
    if (!g_audio_ready) return;
    if (g_current_music == AUDIO_MUSIC_NONE) return;
    if (g_music_valid[g_current_music] &&
        IsMusicStreamPlaying(g_music[g_current_music]))
        StopMusicStream(g_music[g_current_music]);
    g_current_music = AUDIO_MUSIC_NONE;
}

/* ════════════════════════════════════════════════════════════════
   VOLUMES
   ════════════════════════════════════════════════════════════════ */
void audio_set_master_volume(float v) { g_master_volume = clamp01(v); audio_apply_volumes(); }
void audio_set_music_volume(float v)  { g_music_volume  = clamp01(v); audio_apply_volumes(); }
void audio_set_sfx_volume(float v)    { g_sfx_volume    = clamp01(v); audio_apply_volumes(); }
float audio_get_master_volume(void)   { return g_master_volume; }
float audio_get_music_volume(void)    { return g_music_volume;  }
float audio_get_sfx_volume(void)      { return g_sfx_volume;    }