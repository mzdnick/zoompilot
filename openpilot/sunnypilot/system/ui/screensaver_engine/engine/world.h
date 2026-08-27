#pragma once
#include "raylib.h"
#include <stdint.h>
#include <stddef.h>

void world_init(void);
void world_update(float sCam, float dt);
// full-state snapshot (state.c)
size_t world_state_size(void);
void   world_state_save(void *dst);
void   world_state_load(const void *src);
void world_draw(float sCam);
void world_city_glow(float sCam, Vector3 *pos, float *amt);
void world_drop_counts(int *scenery);   // pool-full drops, for --stats
