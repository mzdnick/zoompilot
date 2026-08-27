#pragma once
#include "raylib.h"
#include <stdint.h>
#include <stddef.h>

enum { W_CLEAR, W_RAIN, W_FOG, W_SNOW };

void weather_init(uint64_t seed, int forceKind);
void weather_update(float dt);
// full-state snapshot (state.c)
size_t weather_state_size(void);
void   weather_state_save(void *dst);
void   weather_state_load(const void *src);
float weather_fog_mul(void);
float weather_rain(void);
float weather_snow(void);
float weather_wet(void);
float weather_ground_snow(void);
float weather_dim(void);
void weather_draw(Camera3D cam, float dt);
const char *weather_name(void);
