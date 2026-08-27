#pragma once
#include "raylib.h"
#include <stdint.h>
#include <stddef.h>

typedef struct {
    Vector3 sunDir;
    float   sunElev;       // radians, negative at night
    Color   sunCol;
    float   sunI;
    Color   ambient;
    Color   zenith;
    Color   horizon;
    Color   fogColor;
    float   fogDensity;
    float   stars;
    float   lightsOn;      // 0 day, 1 night
} EnvLight;

extern EnvLight envl;
extern float envIndoor;    // 0 outside, 1 in tunnel; road module drives this

void env_init(uint64_t seed, float dayLength, float startT);
void env_update(float dt);
// full-state snapshot (state.c); includes envIndoor
size_t environment_state_size(void);
void   environment_state_save(void *dst);
void   environment_state_load(const void *src);
void env_draw_sky(Camera3D cam, float sCam, float dt);
