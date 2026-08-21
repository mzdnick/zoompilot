#pragma once
#include "raylib.h"
#include <stdint.h>

typedef struct VehInfo {
    Vector3 pos, fwd, right;
    float s, lat, v, len, w, h;
    int dir, lane, type, braking;
    float detect;      // ADAS fade 0..1
    float distAhead;   // s - egoS
} VehInfo;

void  traffic_init(uint64_t seed);
void  traffic_update(float dt);
void  traffic_set_showcase(int on);   // park one of each vehicle ahead for QA
float traffic_ego_s(void);
float traffic_ego_v(void);
float traffic_ego_lat(void);
int   traffic_ego_target_lane(void);
int   traffic_ego_blinker(void);        // -1 left, +1 right, 0 centered
void  traffic_draw(void);
int   traffic_query(VehInfo *out, int max);
