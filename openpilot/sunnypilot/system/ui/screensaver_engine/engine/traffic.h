#pragma once
#include "raylib.h"
#include <stdint.h>
#include <stddef.h>

#define NPC_MAX 56

typedef struct VehInfo {
    Vector3 pos, fwd, right;
    float lat, len, w, h;
    int dir;
    float detect;      // ADAS fade 0..1
    float distAhead;   // s - egoS
} VehInfo;

void  traffic_init(uint64_t seed);
void  traffic_update(float dt);
// full-state snapshot (state.c)
size_t traffic_state_size(void);
void   traffic_state_save(void *dst);
void   traffic_state_load(const void *src);
void  traffic_set_showcase(int on);   // park one of each vehicle ahead for QA
float traffic_ego_s(void);
float traffic_ego_v(void);
float traffic_ego_lat(void);
int   traffic_ego_target_lane(void);
int   traffic_ego_blinker(void);        // -1 left, +1 right, 0 centered
void  traffic_draw(void);
int   traffic_query(VehInfo *out, int max);
int   traffic_spawn_group(int type, int n, float cruise, int lane, int dir,
                          const Color *cols);   // scripted group for rare events
