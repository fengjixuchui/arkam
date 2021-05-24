#if !defined(__SDL_FMSYNTH_H__)
#define __SDL_FMSYNTH_H__

#include "arkam.h"
#include "shorthands.h"
#include <stdint.h>
#include <math.h>
#include <SDL2/SDL.h>

#define FM_OPERATORS 4
#define FM_VOICES 8
#define FM_WAVE_TABLE_SIZE 22500
#define FM_ENV_TABLE_SIZE 22500


extern double fm_sine_table[FM_WAVE_TABLE_SIZE];
extern double fm_saw_table[FM_WAVE_TABLE_SIZE];
extern double fm_sq_table[FM_WAVE_TABLE_SIZE];
extern double fm_noise_table[FM_WAVE_TABLE_SIZE];

extern double fm_env_table_ed[FM_ENV_TABLE_SIZE]; // early  down
extern double fm_env_table_eu[FM_ENV_TABLE_SIZE]; // early  up
extern double fm_env_table_ld[FM_ENV_TABLE_SIZE]; // lately down
extern double fm_env_table_lu[FM_ENV_TABLE_SIZE]; // lately up


ArkamCode handleFMSYNTH(ArkamVM* vm, Cell op);
void setup_fmsynth(ArkamVM* vm);


#endif
