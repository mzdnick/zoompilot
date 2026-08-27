#pragma once
#include "raylib.h"
#include "raymath.h"
#include <stdint.h>
#include <stddef.h>

#define SEG_LEN         4.0f
#define RING_SEGS       512
#define MASK            (RING_SEGS - 1)
#define DRAW_BACK_SEGS  12
#define DRAW_AHEAD_SEGS 220

#define MEDIAN_HALF 4.0f
#define SHOULDER_L  1.0f
#define SHOULDER_R  2.6f
#define LANE_W      3.6f
#define PAVE_OUT    (MEDIAN_HALF + SHOULDER_L + 2.0f*LANE_W + SHOULDER_R)   // 14.8
// lateral start of the first terrain row (world.c TLAT[0]); drawn pavement
// reaches this far so no see-through slot opens between road and landscape
#define TERRAIN_EDGE 15.6f
#define LANE_FAST   (MEDIAN_HALF + SHOULDER_L + LANE_W*0.5f)                 // 6.8
#define LANE_SLOW   (MEDIAN_HALF + SHOULDER_L + LANE_W*1.5f)                 // 10.4

enum { ZN_PLAINS, ZN_HILLS, ZN_FOREST, ZN_MOUNTAIN, ZN_CANYON, ZN_CITY, ZN_LAKESIDE,
       ZN_COASTAL, ZN_COUNT };

#define SEG_TUNNEL       0x01
#define SEG_BRIDGE       0x02
#define SEG_CITY         0x04
#define SEG_CONSTRUCTION 0x08
#define SEG_GAS          0x10
#define SEG_OVERPASS     0x20
#define SEG_GANTRY       0x40
#define SEG_LIGHTHOUSE   0x80

typedef struct Seg {
    Vector3  pos;      // median center, road elevation
    Vector3  fwd;      // unit forward
    Vector3  right;    // unit right, banked
    Vector3  up;       // unit up, banked
    float    s;
    uint32_t seed;
    uint8_t  flags;
    uint8_t  zone;
    float    terraAmp; // terrain height scale, zone-blended
    float    lakeW;    // lakeside water-side weight, zone-blended
} Seg;

void    road_init(uint64_t seed);
void    road_update(float sCam);
// full-state snapshot (state.c)
size_t  road_state_size(void);
void    road_state_save(void *dst);
void    road_state_load(const void *src);
Seg    *road_seg(int gi);                 // NULL outside ring window
int     road_ring_head(void);             // next global seg index to generate
Vector3 road_point(float s, float lat, float h);
void    road_frame(float s, Vector3 *pos, Vector3 *fwd, Vector3 *right, Vector3 *up);
// frame at a lane offset; dir -1 mirrors fwd/right for oncoming traffic
void    road_frame_at(float s, float lat, int dir,
                      Vector3 *pos, Vector3 *fwd, Vector3 *right, Vector3 *up);
float   road_curv_at(float s);
uint8_t road_flags_at(float s);
int     road_zone_at(float s);
void    road_debug_zones(void);   // print zone layout to stdout
void    road_zone_stats(int *capDrops, int *overQueries);   // for --stats

// segment-local point: pos + right*lat + up*h (shared by road and world draw)
static inline Vector3 xpt(Seg *g, float lat, float h){
    return Vector3Add(g->pos, Vector3Add(Vector3Scale(g->right, lat), Vector3Scale(g->up, h)));
}
void    road_draw(float sCam);
