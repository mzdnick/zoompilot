#pragma once
#include "raylib.h"
#include <stdint.h>

enum { W_CLEAR, W_RAIN, W_FOG, W_SNOW };

void weather_init(uint64_t seed, int forceKind);
void weather_update(float dt);
float weather_fog_mul(void);
float weather_rain(void);
float weather_snow(void);
float weather_wet(void);
float weather_ground_snow(void);
float weather_dim(void);
void weather_draw(Camera3D cam, float dt);
const char *weather_name(void);
