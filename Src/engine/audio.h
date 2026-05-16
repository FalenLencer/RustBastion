#pragma once

#include "../map/theme.h"

typedef enum {
    AUDIO_MUSIC_NONE = -1,
    AUDIO_MUSIC_WASTELAND = 0,
    AUDIO_MUSIC_SWAMP,
    AUDIO_MUSIC_DESERT,
    AUDIO_MUSIC_CITY,
    AUDIO_MUSIC_FACTORY,
    AUDIO_MUSIC_COUNT,
} AudioMusicID;

typedef enum {
    AUDIO_SFX_MENU_CLICK = 0,
    AUDIO_SFX_MENU_CONFIRM,
    AUDIO_SFX_TOWER_PLACE,
    AUDIO_SFX_TOWER_FIRE_GUN,
    AUDIO_SFX_TOWER_FIRE_SNIPER,
    AUDIO_SFX_TOWER_FIRE_FLAME,
    AUDIO_SFX_TOWER_FIRE_TESLA,
    AUDIO_SFX_UNIT_SPAWN,
    AUDIO_SFX_ENEMY_SPAWN,
    AUDIO_SFX_ENEMY_HIT,
    AUDIO_SFX_ENEMY_DEATH,
    AUDIO_SFX_WAVE_START,
    AUDIO_SFX_GAME_OVER,
    AUDIO_SFX_VICTORY,
    AUDIO_SFX_MATERIAL_COLLECT,
    AUDIO_SFX_MATERIAL_APPLY,
    AUDIO_SFX_COUNT,
} AudioSfxID;

int  audio_init(void);
void audio_update(void);
void audio_shutdown(void);
void audio_play_sfx(AudioSfxID id);
void audio_play_theme_music(ThemeID theme);
void audio_stop_music(void);
void audio_set_master_volume(float volume);
void audio_set_music_volume(float volume);
void audio_set_sfx_volume(float volume);
float audio_get_master_volume(void);
float audio_get_music_volume(void);
float audio_get_sfx_volume(void);
