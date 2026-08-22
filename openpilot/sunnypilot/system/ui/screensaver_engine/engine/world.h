#pragma once
#include "raylib.h"
#include <stdint.h>

void world_init(void);
void world_update(float sCam);
void world_draw(float sCam);
void world_city_glow(float sCam, Vector3 *pos, float *amt);
