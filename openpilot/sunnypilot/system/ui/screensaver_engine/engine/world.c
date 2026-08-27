#include "world.h"
#include "road.h"
#include "util.h"
#include "rendering.h"
#include "environment.h"
#include "weather.h"
#include <string.h>

#define TROWS   10
#define SCENY_MAX 720

static const float TLAT[TROWS] = { TERRAIN_EDGE, 19.0f, 26.0f, 36.0f, 52.0f, 82.0f, 128.0f, 205.0f, 330.0f, 520.0f };
static const float TPROF[TROWS] = { 0.03f, 0.10f, 0.28f, 0.50f, 0.75f, 1.00f, 1.35f, 1.80f, 2.35f, 3.00f };
static const float TTUN[TROWS]  = { 9.0f, 15.0f, 24.0f, 36.0f, 52.0f, 70.0f, 92.0f, 114.0f, 136.0f, 160.0f };
static const float TCAN[TROWS]  = { -2.5f, -6.0f, -14.0f, -30.0f, -55.0f, -78.0f, -96.0f, -106.0f, -110.0f, -112.0f };
static const float TLAK[TROWS]  = { -0.5f, -1.3f, -1.8f, -2.0f, -2.1f, -2.15f, -2.2f, -2.25f, -2.3f, -2.35f };

enum { SC_PINE, SC_OAK, SC_BUSH, SC_ROCK, SC_POLE, SC_LAMP, SC_SIGN_DIR, SC_SIGN_SPEED,
       SC_BILLBOARD, SC_BLD, SC_SHED, SC_CONE, SC_BARRIER, SC_GAS, SC_DOCK, SC_LIGHTHOUSE };

typedef struct {
    uint8_t  type;
    int32_t  seg;
    float    lat, scale, rot, h, w, d;
    uint32_t seed;
    // baked lit-window grid (max 14 rows x 9 cols), filled by scen_push_bld
    uint8_t  winOn[16], winWarm[16];
} Scen;

static struct {
    float    th[RING_SEGS][TROWS][2];
    Scen     sc[SCENY_MAX];
    int      scN;
    int      polled;
    float    cityS[4][2];   // city ranges [s0,s1] for skyline glow
    int      cityN;
    int      winBudget;
    int      dropScen;      // scenery pool-full drops, for --stats
    float    clock;         // sim-time clock for animated scenery (lighthouse sweep)
} W;

size_t world_state_size(void){ return sizeof W; }
void   world_state_save(void *dst){ memcpy(dst, &W, sizeof W); }
void   world_state_load(const void *src){ memcpy(&W, src, sizeof W); }

// -------- terrain heights per new segment --------

static void terrain_new(int i, Seg *g){
    float s = g->s;
    for (int side = 0; side < 2; side++){
        for (int r = 0; r < TROWS; r++){
            float h;
            if (g->flags & SEG_TUNNEL){
                h = TTUN[r]*(0.90f + 0.25f*vnoise1(s*0.01f + side*7.7f + r*0.9f));
            } else if (g->flags & SEG_BRIDGE){
                h = TCAN[r]*(0.92f + 0.16f*vnoise1(s*0.012f + side*5.3f + r*0.7f));
            } else {
                float n1 = fbm1(s*0.0038f + side*3.7f + r*0.55f, 3);
                float n2 = vnoise1(s*0.020f + r*7.1f + side*11.0f);
                h = g->terraAmp*TPROF[r]*(0.60f + 0.80f*n1 + 0.30f*n2*(r < 3 ? 1.0f : 0.35f));
            }
            // lakeside: the left side sinks to a calm water plane
            if (g->lakeW > 0.001f && side == 1){
                float wl = TLAK[r] + 0.08f*vnoise1(s*0.05f + r*1.3f);
                h = lerpf(h, wl, g->lakeW);
            }
            // ramp toward mountain shape before a tunnel so terrain closes over it
            Seg *t4 = road_seg(i + 4), *t10 = road_seg(i + 10), *t16 = road_seg(i + 16);
            float w = 0.0f;
            if (t16 && (t16->flags & SEG_TUNNEL)) w = 0.55f;
            if (t10 && (t10->flags & SEG_TUNNEL)) w = 0.8f;
            if (t4  && (t4->flags & SEG_TUNNEL))  w = 1.0f;
            if (w > 0.0f && !(g->flags & SEG_TUNNEL))
                h = lerpf(h, TTUN[r]*(0.9f + 0.25f*vnoise1(s*0.01f + side*7.7f + r*0.9f)), w);
            W.th[i & MASK][r][side] = h;
        }
    }
}

// -------- scenery spawn per new segment --------

static void scen_push(uint8_t type, int seg, float lat, float scale, float rot, uint32_t seed){
    if (W.scN >= SCENY_MAX){ W.dropScen++; return; }
    Scen *x = &W.sc[W.scN++];
    x->type = type; x->seg = seg; x->lat = lat; x->scale = scale; x->rot = rot;
    x->h = 0.0f; x->w = 0.0f; x->d = 0.0f; x->seed = seed;
}

// fixed-size furniture: unit scale, no rotation
static void scen_item(uint8_t type, int seg, float lat, uint32_t seed){
    scen_push(type, seg, lat, 1.0f, 0.0f, seed);
}

// buildings are the only item with custom dimensions
static void scen_push_bld(int seg, float lat, float rot, float h, float w, float d, uint32_t seed){
    if (W.scN >= SCENY_MAX){ W.dropScen++; return; }
    Scen *x = &W.sc[W.scN++];
    x->type = SC_BLD; x->seg = seg; x->lat = lat; x->scale = 1.0f; x->rot = rot;
    x->h = h; x->w = w; x->d = d; x->seed = seed;
    // bake the lit-window pattern at spawn: same hash per cell as before,
    // just computed once instead of per window per frame
    memset(x->winOn, 0, sizeof(x->winOn));
    memset(x->winWarm, 0, sizeof(x->winWarm));
    for (int wr = 0; wr < 14; wr++){
        for (int wc = 0; wc < 9; wc++){
            uint32_t hsh = hash_u32(seed + (uint32_t)wr*31u + (uint32_t)wc*7u);
            if ((hsh & 3) >= 2) continue;
            int idx = wr*9 + wc;
            x->winOn[idx >> 3] |= (uint8_t)(1u << (idx & 7));
            if (hsh & 8) x->winWarm[idx >> 3] |= (uint8_t)(1u << (idx & 7));
        }
    }
}

// side-of-road coin flip. Polarity is load-bearing for the seeded stream:
// rng_lat maps chance-true to +lat; the reed site negates it. Same draws,
// same order.
static float rng_lat(Rng *r, float lo, float hi){
    return (rng_chance(r, 50) ? 1.0f : -1.0f)*rng_range(r, lo, hi);
}

static void scenery_new(int i, Seg *g){
    Rng r; r.s = mix_seed((uint64_t)g->seed, 0x51CE);
    int zt = g->zone;
    uint8_t fl = g->flags;

    if (fl & SEG_LIGHTHOUSE)
        scen_item(SC_LIGHTHOUSE, i, -19.0f, g->seed ^ 0x51EAu);   // out on the water side

    if (fl & SEG_GAS){
        scen_item(SC_GAS, i, 27.0f, g->seed);
        return;   // keep the station area clear
    }
    if (fl & SEG_CONSTRUCTION){
        if ((i % 3) == 0) scen_item(SC_CONE, i, 14.3f, g->seed ^ 0xC0);
        if ((i % 2) == 0) scen_item(SC_BARRIER, i, 13.1f, g->seed ^ 0xBA);
        return;
    }

    if (zt == ZN_CITY){
        for (int side = 0; side < 2; side++){
            for (int a = 0; a < 2; a++){
                if (!rng_chance(&r, 55)) continue;
                float lat = (side ? -1.0f : 1.0f)*rng_range(&r, 27.0f, 68.0f);
                float h = 10.0f + rng_f(&r)*rng_f(&r)*58.0f;
                float w2 = rng_range(&r, 9.0f, 24.0f), d2 = rng_range(&r, 9.0f, 20.0f);
                scen_push_bld(i, lat, rng_range(&r, -0.5f, 0.5f), h, w2, d2, rng_u64(&r));
            }
        }
        if ((i % 10) == 0)
            scen_item(SC_LAMP, i, ((i % 20) == 0) ? 15.8f : -15.8f, g->seed ^ 0x1A);
        if (rng_chance(&r, 2))
            scen_item(SC_BILLBOARD, i, rng_lat(&r, 24.0f, 34.0f), rng_u64(&r));
        return;
    }

    // rural scenery
    int trees = 0, rocks = 0;
    switch (zt){
        case ZN_PLAINS:  trees = rng_int(&r, 3); rocks = rng_int(&r, 2); break;
        case ZN_HILLS:   trees = 1 + rng_int(&r, 3); rocks = rng_int(&r, 2); break;
        case ZN_FOREST:  trees = 3 + rng_int(&r, 4); break;
        case ZN_MOUNTAIN:trees = rng_int(&r, 3); rocks = 1 + rng_int(&r, 3); break;
        case ZN_CANYON:  rocks = rng_int(&r, 2); trees = rng_int(&r, 2); break;
        case ZN_LAKESIDE:trees = 1 + rng_int(&r, 2); rocks = rng_int(&r, 2); break;
        case ZN_COASTAL: trees = rng_int(&r, 2); rocks = rng_int(&r, 3); break;
        default: break;
    }
    float latMin = (zt == ZN_FOREST) ? 16.5f : 19.0f;
    float latMax = (zt == ZN_FOREST) ? 80.0f : 120.0f;
    for (int k = 0; k < trees; k++){
        // zones with water on the left keep that side clear: trees grow on
        // the land side only
        int side = (zt == ZN_LAKESIDE || zt == ZN_COASTAL) ? 0 : (rng_chance(&r, 50) ? 0 : 1);
        float lat = (side ? -1.0f : 1.0f)*rng_range(&r, latMin, latMax);
        int oak = rng_chance(&r, 40);
        // trees never rotate, so no rotation is drawn (rot stays building-only)
        scen_push(oak ? SC_OAK : SC_PINE, i, lat, rng_range(&r, 0.8f, 1.5f), 0.0f, rng_u64(&r));
    }
    for (int k = 0; k < rocks; k++){
        int side = rng_chance(&r, 50) ? 0 : 1;
        scen_push(SC_ROCK, i, (side ? -1.0f : 1.0f)*rng_range(&r, 16.0f, 140.0f),
                  rng_range(&r, 0.5f, 2.2f), 0.0f, rng_u64(&r));
    }
    if (zt == ZN_COASTAL){
        // dune grass stays on the land side
        if (rng_chance(&r, 30))
            scen_push(SC_BUSH, i, rng_range(&r, 16.0f, 50.0f), rng_range(&r, 0.5f, 1.0f), 0.0f, rng_u64(&r));
    } else if (rng_chance(&r, 30))
        scen_push(SC_BUSH, i, -rng_lat(&r, 15.5f, 40.0f), rng_range(&r, 0.7f, 1.3f), 0.0f, rng_u64(&r));
    // reeds on the lakeside shore, a plank dock every so often
    if (zt == ZN_LAKESIDE && rng_chance(&r, 40))
        scen_push(SC_BUSH, i, -rng_range(&r, 16.0f, 20.0f), rng_range(&r, 0.8f, 1.4f), 0.0f, rng_u64(&r));
    if (zt == ZN_LAKESIDE && (i % 15) == 7)
        scen_item(SC_DOCK, i, -17.5f, g->seed ^ 0xD0Cu);
    if ((i % 10) == 5)
        scen_item(SC_POLE, i, ((i / 10) & 1) ? 17.6f : -17.6f, g->seed ^ 0x90);
    if (rng_chance(&r, 3))
        scen_item(SC_SIGN_DIR, i, (rng_chance(&r, 50) ? 16.8f : -16.8f), rng_u64(&r));
    else if (rng_chance(&r, 1))
        scen_item(SC_SIGN_SPEED, i, 16.8f, rng_u64(&r));
    if (rng_chance(&r, 1) && (zt == ZN_PLAINS || zt == ZN_HILLS))
        scen_item(SC_BILLBOARD, i, rng_lat(&r, 26.0f, 40.0f), rng_u64(&r));
    if (rng_chance(&r, 2) && zt == ZN_PLAINS)
        scen_item(SC_SHED, i, rng_lat(&r, 30.0f, 60.0f), rng_u64(&r));
}

void world_init(void){
    memset(&W, 0, sizeof(W));
}

void world_update(float sCam, float dt){
    W.clock += dt;
    // poll new segments
    for (int i = W.polled; i < road_ring_head(); i++){
        Seg *g = road_seg(i);
        if (!g) break;
        terrain_new(i, g);
        scenery_new(i, g);
        if (g->zone == ZN_CITY){
            if (W.cityN > 0 && g->s <= W.cityS[W.cityN-1][1] + 60.0f){
                W.cityS[W.cityN-1][1] = g->s + 4.0f;
            } else if (W.cityN < 4){
                W.cityS[W.cityN][0] = g->s;
                W.cityS[W.cityN][1] = g->s + 4.0f;
                W.cityN++;
            }
        }
        W.polled = i + 1;
    }
    // recycle scenery behind the camera
    for (int k = 0; k < W.scN; ){
        if ((float)W.sc[k].seg*SEG_LEN < sCam - 70.0f)
            W.sc[k] = W.sc[--W.scN];
        else k++;
    }
}

void world_city_glow(float sCam, Vector3 *pos, float *amt){
    *amt = 0.0f;
    *pos = road_point(sCam + 1500.0f, 0.0f, 30.0f);
    for (int i = 0; i < W.cityN; i++){
        float mid = 0.5f*(W.cityS[i][0] + W.cityS[i][1]);
        float d = mid - sCam;
        if (d < 200.0f || d > 3600.0f) continue;
        float a = 1.0f - d/3600.0f;
        if (a > *amt){ *amt = a; *pos = road_point(mid, 0.0f, 28.0f); }
    }
}

void world_drop_counts(int *scenery){ *scenery = W.dropScen; }

// -------- drawing --------

// ground-snow fade toward a per-item snow color
static Color snow_tint(Color c, Color snowCol, float amt){
    return col_lerp(c, snowCol, weather_ground_snow()*amt);
}

static Color terrain_color(Seg *g, int r, int side, float h, float latAbs, float snow){
    static const int GRASS[ZN_COUNT][3] = {
        { 66, 72, 44 }, { 58, 68, 44 }, { 36, 50, 36 }, { 52, 60, 46 }, { 98, 78, 60 }, { 52, 52, 55 },
        { 64, 76, 50 }, { 96, 102, 64 }
    };
    int zt = g->zone;
    Color c = { (unsigned char)GRASS[zt][0], (unsigned char)GRASS[zt][1], (unsigned char)GRASS[zt][2], 255 };

    // lakeside water side: grass verge, sand strip, then still water
    if (g->lakeW > 0.001f && side == 1){
        Color lake = (r == 1) ? (Color){ 126, 116, 92, 255 }
                  : (r < 5)  ? (Color){ 22, 38, 62, 255 }
                             : (Color){ 16, 30, 52, 255 };
        c = col_lerp(c, lake, g->lakeW);
    }

    if (zt == ZN_PLAINS && latAbs > 60.0f){
        uint32_t cell = hash_u32((uint32_t)(g->s/90.0f)*3u + (uint32_t)(latAbs/70.0f)*7u + (uint32_t)side);
        if (cell & 1) c = (Color){ 92, 84, 48, 255 };
        else c = (Color){ 58, 74, 42, 255 };
    }
    if (zt == ZN_MOUNTAIN && h > 26.0f) c = (Color){ 88, 84, 78, 255 };
    if (zt == ZN_CANYON){
        int band = ((int)(h/6.0f)) & 1;
        c = band ? (Color){ 98, 78, 60, 255 } : (Color){ 84, 66, 52, 255 };
        if (r >= 8) c = (Color){ 26, 34, 46, 255 };   // water at the canyon floor
    }
    if (snow > 0.01f) c = col_lerp(c, (Color){ 216, 220, 228, 255 }, snow*0.85f);
    if (zt == ZN_MOUNTAIN && h > 44.0f) c = col_lerp(c, (Color){ 224, 228, 236, 255 }, clampf((h - 44.0f)/22.0f, 0.0f, 1.0f));
    return c;
}

static void draw_terrain(int camI){
    double now = GetTime();   // one clock read for all water glints
    float snow = weather_ground_snow();
    for (int i = camI - DRAW_BACK_SEGS; i <= camI + DRAW_AHEAD_SEGS; i++){
        Seg *A = road_seg(i), *B = road_seg(i + 1);
        if (!A || !B) break;
        for (int side = 0; side < 2; side++){
            float sg = side ? -1.0f : 1.0f;
            // embankment face: the first terrain row can sit below the road
            // plane (signed noise), so seal the seam with a vertical skirt
            // from the pavement edge down to the terrain edge
            {
                float e0 = W.th[i & MASK][0][side]     - 0.10f;
                float e1 = W.th[(i + 1) & MASK][0][side] - 0.10f;
                float b0 = fminf(e0, 0.0f), b1 = fminf(e1, 0.0f);
                if (b0 < -0.005f || b1 < -0.005f){
                    float lat = sg*TERRAIN_EDGE;
                    Color sc = col_scale(terrain_color(A, 0, side, 0.5f*(b0 + b1), TERRAIN_EDGE, snow), 0.72f);
                    if (sg > 0)
                        geo_quad(xpt(A, lat, 0.0f), xpt(A, lat, b0),
                                 xpt(B, lat, b1), xpt(B, lat, 0.0f), sc, 0.0f);
                    else
                        geo_quad(xpt(A, lat, b0), xpt(A, lat, 0.0f),
                                 xpt(B, lat, 0.0f), xpt(B, lat, b1), sc, 0.0f);
                }
            }
            for (int r = 0; r < TROWS - 1; r++){
                float ha0 = W.th[i & MASK][r][side],     hb0 = W.th[(i + 1) & MASK][r][side];
                float ha1 = W.th[i & MASK][r + 1][side], hb1 = W.th[(i + 1) & MASK][r + 1][side];
                float la = sg > 0 ? TLAT[r] : -TLAT[r + 1];
                float lb = sg > 0 ? TLAT[r + 1] : -TLAT[r];
                float hA0 = sg > 0 ? ha0 : ha1, hB0 = sg > 0 ? hb0 : hb1;
                float hA1 = sg > 0 ? ha1 : ha0, hB1 = sg > 0 ? hb1 : hb0;
                Color c = terrain_color(A, r, side, 0.5f*(ha0 + ha1), fabsf(0.5f*(la + lb)), snow);
                float emis = 0.0f;
                // water glints: a seeded subset of quads twinkles; time drives
                // only the shimmer phase, never which quads light up
                if (A->lakeW > 0.6f && side == 1 && r >= 2){
                    uint32_t hsh = hash_u32((uint32_t)i*2654435761u ^ (uint32_t)r*97u + 1u);
                    if ((hsh & 7) < 2){
                        float tw = 0.5f + 0.5f*sinf((float)now*(1.2f + 2.0f*lat_f(hsh ^ 5u)) + lat_f(hsh)*6.28f);
                        emis = 0.30f*tw;
                        Color glint = col_lerp((Color){ 205, 224, 250, 255 }, (Color){ 255, 250, 216, 255 },
                                               clampf(envl.sunI, 0.0f, 1.0f));
                        c = col_lerp(c, glint, emis*0.9f);
                    }
                }
                geo_quad(xpt(A, la, hA0 - 0.10f), xpt(A, lb, hA1 - 0.10f),
                         xpt(B, lb, hB1 - 0.10f), xpt(B, la, hB0 - 0.10f), c, emis);
            }
        }
    }
}

static void draw_tree_pine(Vector3 p, float scale, uint32_t seed){
    float snow = weather_ground_snow();
    Color trunk = { 44, 32, 24, 255 };
    Color c1 = col_lerp((Color){ 26, 44, 34, 255 }, (Color){ 200, 208, 214, 255 }, snow*0.4f);
    Color c2 = col_lerp((Color){ 34, 56, 40, 255 }, (Color){ 208, 214, 220, 255 }, snow*0.3f);
    geo_cylinder(p, Vector3Add(p, (Vector3){ 0, 1.4f*scale, 0 }), 0.16f*scale, 0.10f*scale, 5, trunk, 0.0f);
    float r1 = 1.35f*scale*(0.85f + 0.3f*lat_f(seed));
    geo_cone(Vector3Add(p, (Vector3){ 0, 1.0f*scale, 0 }), (Vector3){ 0, 1, 0 }, r1, 2.2f*scale, 7, c1, 0.0f);
    geo_cone(Vector3Add(p, (Vector3){ 0, 2.2f*scale, 0 }), (Vector3){ 0, 1, 0 }, r1*0.72f, 1.9f*scale, 7, c2, 0.0f);
}

static void draw_tree_oak(Vector3 p, float scale, uint32_t seed){
    Color trunk = { 48, 36, 26, 255 };
    Color c = snow_tint((Color){ 40, 58, 36, 255 }, (Color){ 202, 208, 212, 255 }, 0.35f);
    c = col_scale(c, 0.85f + 0.3f*lat_f(seed ^ 7u));
    geo_cylinder(p, Vector3Add(p, (Vector3){ 0, 1.5f*scale, 0 }), 0.20f*scale, 0.13f*scale, 5, trunk, 0.0f);
    Vector3 ax = { 0, 1, 0 };
    geo_cone(Vector3Add(p, (Vector3){ 0, 1.9f*scale, 0 }), ax, 1.35f*scale, 1.5f*scale, 6, c, 0.0f);
    geo_cone(Vector3Add(p, (Vector3){ 0, 2.7f*scale, 0 }), ax, 0.95f*scale, 1.1f*scale, 6, col_scale(c, 1.1f), 0.0f);
}

static void draw_building(Scen *x, Seg *g, float dist){
    Vector3 c = xpt(g, x->lat, x->h*0.5f);
    float ca = cosf(x->rot), sa = sinf(x->rot);
    Vector3 rx = Vector3Normalize((Vector3){ g->right.x*ca + g->fwd.x*sa, 0, g->right.z*ca + g->fwd.z*sa });
    Vector3 fz = Vector3Normalize((Vector3){ g->fwd.x*ca - g->right.x*sa, 0, g->fwd.z*ca - g->right.z*sa });
    Color bc = col_scale((Color){ 96, 94, 96, 255 }, 0.8f + 0.4f*lat_f(x->seed));
    bc = col_lerp(bc, (Color){ 120, 116, 112, 255 }, 0.3f*lat_f(x->seed ^ 3u));
    geo_box(c, rx, (Vector3){ 0, 1, 0 }, fz, x->w*0.5f, x->h*0.5f, x->d*0.5f, bc, 0.0f);

    float night = envl.lightsOn;
    if (night > 0.05f && dist < 340.0f && W.winBudget > 0){
        int rows = (int)(x->h/3.2f), cols = (int)(x->w/2.6f);
        if (rows > 14) rows = 14;
        if (cols > 9) cols = 9;
        int side = x->lat > 0 ? -1 : 1;
        Vector3 faceN = Vector3Scale(rx, (float)side);
        Vector3 faceC = Vector3Add(c, Vector3Scale(faceN, x->w*0.5f + 0.06f));
        for (int wr = 0; wr < rows && W.winBudget > 0; wr++){
            for (int wc = 0; wc < cols; wc++){
                int idx = wr*9 + wc;
                if (!(x->winOn[idx >> 3] & (1u << (idx & 7)))) continue;
                Color wc1 = (x->winWarm[idx >> 3] & (1u << (idx & 7)))
                          ? (Color){ 255, 214, 150, 255 } : (Color){ 200, 226, 255, 255 };
                Vector3 wp = Vector3Add(faceC,
                    Vector3Add(Vector3Scale(fz, (float)(wc - cols/2)*2.6f),
                               Vector3Scale((Vector3){ 0, 1, 0 }, 2.2f + (float)wr*3.2f - x->h*0.5f)));
                Vector3 a = Vector3Add(Vector3Add(wp, Vector3Scale(fz, -0.75f)), (Vector3){ 0, -0.85f, 0 });
                Vector3 b = Vector3Add(Vector3Add(wp, Vector3Scale(fz,  0.75f)), (Vector3){ 0, -0.85f, 0 });
                Vector3 cc = Vector3Add(b, (Vector3){ 0, 1.7f, 0 });
                Vector3 d = Vector3Add(a, (Vector3){ 0, 1.7f, 0 });
                geo_quad(a, b, cc, d, wc1, 0.85f);
                W.winBudget--;
            }
        }
    }
}

static void draw_lamp(Scen *x, Seg *g){
    float side = x->lat > 0 ? 1.0f : -1.0f;
    Vector3 base = xpt(g, x->lat, 0.0f);
    Vector3 top = Vector3Add(base, (Vector3){ 0, 8.0f, 0 });
    Vector3 armEnd = Vector3Add(top, Vector3Scale(g->right, -3.2f*side));
    Color pc = { 70, 72, 76, 255 };
    geo_cylinder(base, top, 0.09f, 0.06f, 5, pc, 0.0f);
    geo_cylinder(top, armEnd, 0.05f, 0.05f, 4, pc, 0.0f);
    float night = envl.lightsOn;
    Color lc = night > 0.05f ? (Color){ 255, 220, 160, 255 } : (Color){ 60, 62, 66, 255 };
    geo_box(armEnd, g->right, g->up, g->fwd, 0.30f, 0.08f, 0.14f, lc, night*0.9f);
    if (night > 0.05f){
        glow_add(Vector3Add(armEnd, (Vector3){ 0, -0.15f, 0 }), (Color){ 255, 208, 140, 255 }, 0.85f, 1.0f, 0.20f);
        flatspot_add(xpt(g, x->lat - 3.2f*side, 0.08f), g->right, g->fwd, 5.5f, 7.0f,
                     (Color){ 255, 214, 150, 255 }, 0.085f);
        refl_add(xpt(g, x->lat - 3.2f*side, 0.06f), (Color){ 255, 208, 140, 255 },
                 0.95f, 4.2f, 0.12f);
    }
}

static void draw_gas(Scen *x, Seg *g){
    (void)x;
    Vector3 fwd = g->fwd, right = g->right, up = g->up;
    Color roof = { 150, 60, 55, 255 }, wall = { 180, 178, 172, 255 }, pole = { 120, 120, 124, 255 };
    float lat0 = 24.0f, lat1 = 40.0f, s0 = -8.0f, s1 = 8.0f;
    Vector3 p11 = xpt(g, lat0, 0), p12 = xpt(g, lat1, 0);
    Vector3 p21 = Vector3Add(p11, Vector3Scale(fwd, s0)), p22 = Vector3Add(p12, Vector3Scale(fwd, s0));
    Vector3 p31 = Vector3Add(p11, Vector3Scale(fwd, s1)), p32 = Vector3Add(p12, Vector3Scale(fwd, s1));
    Vector3 leg[4] = { p21, p22, p31, p32 };
    for (int k = 0; k < 4; k++)
        geo_cylinder(leg[k], Vector3Add(leg[k], (Vector3){ 0, 5.2f, 0 }), 0.16f, 0.14f, 6, pole, 0.0f);
    Vector3 roofC = xpt(g, 0.5f*(lat0 + lat1), 5.45f);
    Vector3 rf = Vector3Add(roofC, Vector3Scale(fwd, 0.5f*(s0 + s1)));
    geo_box(rf, right, up, fwd, 0.5f*(lat1 - lat0) + 1.2f, 0.28f, 0.5f*(s1 - s0) + 1.2f, roof, 0.0f);
    // building
    geo_box(xpt(g, lat1 + 9.0f, 2.2f), right, up, fwd, 5.0f, 2.2f, 4.4f, wall, 0.0f);
    // pumps
    for (int k = 0; k < 2; k++){
        Vector3 pp = xpt(g, lat0 + 4.0f + (float)k*7.0f, 0.7f);
        geo_box(pp, right, up, fwd, 0.5f, 0.7f, 0.8f, (Color){ 220, 220, 224, 255 }, 0.0f);
    }
    // sign
    Vector3 sp = xpt(g, lat0 - 2.5f, 0.0f);
    geo_cylinder(sp, Vector3Add(sp, (Vector3){ 0, 9.0f, 0 }), 0.20f, 0.12f, 6, pole, 0.0f);
    Vector3 sc = xpt(g, lat0 - 2.5f, 9.9f);
    Vector3 br = Vector3Add(sc, Vector3Add(Vector3Scale(right, 1.7f), Vector3Scale(fwd, 1.7f)));
    Vector3 tl = Vector3Add(sc, Vector3Add(Vector3Scale(right, -1.7f), Vector3Scale(fwd, -1.7f)));
    tl = Vector3Add(tl, (Vector3){ 0, 3.4f, 0 });
    sign_add(SA_FUEL, Vector3Add(tl, (Vector3){ 0, -3.4f, 0 }), br, Vector3Add(br, (Vector3){ 0, 3.4f, 0 }), tl);
    float night = envl.lightsOn;
    if (night > 0.05f){
        glow_add(rf, (Color){ 255, 240, 220, 255 }, 3.2f, 1.0f, 0.06f);
        flatspot_add(xpt(g, 0.5f*(lat0 + lat1), 0.10f), right, fwd, 8.0f, 9.0f,
                     (Color){ 255, 236, 205, 255 }, 0.12f);
        glow_add(sc, (Color){ 120, 200, 255, 255 }, 1.4f, 1.0f, 0.18f);
    }
}

// roadside sign: cylinder pole plus one atlas panel
static void draw_pole_sign(Seg *g, float lat, float poleH, float ra, float rb,
                           float panelY, float hw, float ph, int sa){
    Vector3 base = xpt(g, lat, 0.0f);
    Color pc = { 130, 132, 136, 255 };
    geo_cylinder(base, Vector3Add(base, (Vector3){ 0, poleH, 0 }), ra, rb, 5, pc, 0.0f);
    Vector3 c = Vector3Add(base, (Vector3){ 0, panelY, 0 });
    sign_panel(c, g->right, (Vector3){ 0, 1, 0 }, hw, ph, sa);
}

void world_draw(float sCam){
    int camI = (int)floorf(sCam / SEG_LEN);
    W.winBudget = 900;
    draw_terrain(camI);

    for (int k = 0; k < W.scN; k++){
        Scen *x = &W.sc[k];
        int i = x->seg;
        if (i < camI - DRAW_BACK_SEGS || i > camI + DRAW_AHEAD_SEGS) continue;
        Seg *g = road_seg(i);
        if (!g) continue;
        float latA = fabsf(x->lat);
        if (latA > 30.0f && (float)(i - camI)*SEG_LEN > 480.0f) continue;   // cull far side items

        switch (x->type){
        case SC_PINE: draw_tree_pine(xpt(g, x->lat, -0.3f), x->scale, x->seed); break;
        case SC_OAK:  draw_tree_oak(xpt(g, x->lat, -0.3f), x->scale, x->seed); break;
        case SC_BUSH: {
            Color c = snow_tint((Color){ 44, 60, 40, 255 }, (Color){ 200, 206, 212, 255 }, 0.4f);
            geo_cone(xpt(g, x->lat, -0.2f), (Vector3){ 0, 1, 0 }, 0.7f*x->scale, 0.8f*x->scale, 5, c, 0.0f);
            break; }
        case SC_ROCK: {
            Color c = snow_tint((Color){ 92, 88, 82, 255 }, (Color){ 205, 208, 214, 255 }, 0.5f);
            geo_cone(xpt(g, x->lat, -0.2f), (Vector3){ 0, 1, 0 }, 0.8f*x->scale, 0.9f*x->scale, 4, c, 0.0f);
            geo_cone(Vector3Add(xpt(g, x->lat + 0.6f, -0.2f), (Vector3){ 0, 0.2f, 0 }), (Vector3){ 0, 1, 0 },
                     0.5f*x->scale, 0.6f*x->scale, 4, col_scale(c, 0.85f), 0.0f);
            break; }
        case SC_POLE: {
            Vector3 base = xpt(g, x->lat, 0.0f);
            Vector3 top = Vector3Add(base, (Vector3){ 0, 9.0f, 0 });
            Color pc = { 58, 50, 44, 255 };
            geo_cylinder(base, top, 0.12f, 0.09f, 5, pc, 0.0f);
            Vector3 arm1 = Vector3Add(top, Vector3Scale(g->fwd, 1.1f));
            Vector3 arm2 = Vector3Add(top, Vector3Scale(g->fwd, -1.1f));
            geo_cylinder(Vector3Add(top, (Vector3){ 0, -0.2f, 0 }), Vector3Add(arm1, (Vector3){ 0, -0.2f, 0 }), 0.05f, 0.05f, 4, pc, 0.0f);
            geo_cylinder(Vector3Add(top, (Vector3){ 0, -0.2f, 0 }), Vector3Add(arm2, (Vector3){ 0, -0.2f, 0 }), 0.05f, 0.05f, 4, pc, 0.0f);
            break; }
        case SC_LAMP: draw_lamp(x, g); break;
        case SC_SIGN_DIR:
            draw_pole_sign(g, x->lat, 6.6f, 0.08f, 0.06f, 5.6f, 1.9f, 1.7f,
                           (x->seed & 1) ? SA_EXIT_BIG : SA_EXIT_SMALL);
            break;
        case SC_SIGN_SPEED:
            draw_pole_sign(g, x->lat, 4.6f, 0.06f, 0.05f, 4.1f, 0.75f, 1.5f, SA_SPEED);
            break;
        case SC_BILLBOARD: {
            Vector3 base = xpt(g, x->lat, 0.0f);
            Color pc = { 70, 66, 62, 255 };
            geo_box(Vector3Add(base, (Vector3){ 0, 3.5f, 0 }), g->right, g->up, g->fwd, 0.15f, 3.5f, 0.15f, pc, 0.0f);
            Vector3 c = Vector3Add(base, (Vector3){ 0, 8.6f, 0 });
            sign_panel(c, g->right, (Vector3){ 0, 1, 0 }, 4.2f, 4.2f, SA_BILL1 + (int)(x->seed % 3));
            if (envl.lightsOn > 0.05f)
                glow_add(Vector3Add(c, (Vector3){ 0, -1.5f, 0 }), (Color){ 255, 250, 240, 255 }, 2.6f, 1.6f, 0.05f);
            break; }
        case SC_BLD: {
            float dist = (float)(i - camI)*SEG_LEN;
            draw_building(x, g, dist);
            break; }
        case SC_SHED: {
            geo_box(Vector3Add(xpt(g, x->lat, 1.6f), (Vector3){ 0, 0, 0 }), g->right, g->up, g->fwd,
                    3.0f, 1.6f, 2.4f, (Color){ 96, 66, 52, 255 }, 0.0f);
            break; }
        case SC_CONE:
            geo_cone(xpt(g, x->lat, 0.02f), (Vector3){ 0, 1, 0 }, 0.20f, 0.55f, 5, (Color){ 232, 118, 40, 255 }, 0.08f);
            break;
        case SC_BARRIER:
            geo_box(xpt(g, x->lat, 0.45f), g->right, g->up, g->fwd, 0.9f, 0.45f, 0.2f, (Color){ 226, 226, 228, 255 }, 0.0f);
            break;
        case SC_GAS: draw_gas(x, g); break;
        case SC_DOCK: {
            // plank pier reaching over the water, small lamp at the far end
            Vector3 ax[3] = { g->right, g->up, g->fwd };
            Color wood = { 92, 70, 50, 255 };
            Vector3 c0 = xpt(g, x->lat - 4.5f, -1.2f);
            geo_box(c0, ax[0], ax[1], ax[2], 4.6f, 0.07f, 0.85f, wood, 0.0f);
            for (int j = 0; j < 2; j++){
                float pl = x->lat + (j ? -8.6f : -0.4f);
                for (int z = -1; z <= 1; z += 2){
                    Vector3 b = Vector3Add(xpt(g, pl, -2.6f), Vector3Scale(g->fwd, 0.7f*z));
                    geo_cylinder(b, Vector3Add(b, (Vector3){ 0, 1.45f, 0 }), 0.07f, 0.07f, 4,
                                 col_scale(wood, 0.8f), 0.0f);
                }
            }
            if (envl.lightsOn > 0.05f){
                Vector3 lp = Vector3Add(c0, Vector3Scale(ax[0], -4.4f));
                geo_box(Vector3Add(lp, (Vector3){ 0, 0.75f, 0 }), ax[0], ax[1], ax[2],
                        0.05f, 0.75f, 0.05f, wood, 0.0f);
                glow_add(Vector3Add(lp, (Vector3){ 0, 1.5f, 0 }), (Color){ 255, 214, 150, 255 },
                         0.30f, 1.0f, 0.14f);
            }
            break; }
        case SC_LIGHTHOUSE: {
            // striped tower on a rock shelf at the shore, lantern plus a
            // rotating beam drawn as an additive glow chain
            Vector3 base = xpt(g, x->lat, -0.6f);
            Color white = { 226, 228, 230, 255 }, red = { 178, 62, 52, 255 };
            geo_cylinder(base, Vector3Add(base, (Vector3){ 0, 0.8f, 0 }),
                         3.4f, 2.8f, 8, (Color){ 70, 72, 70, 255 }, 0.0f);
            for (int j = 0; j < 4; j++){
                float y0 = 0.6f + (float)j*2.2f;
                float r0 = 2.0f - 0.22f*(float)j;
                geo_cylinder(Vector3Add(base, (Vector3){ 0, y0, 0 }),
                             Vector3Add(base, (Vector3){ 0, y0 + 2.2f, 0 }),
                             r0, r0 - 0.22f, 8, (j & 1) ? red : white, 0.0f);
            }
            Vector3 top = Vector3Add(base, (Vector3){ 0, 9.4f, 0 });
            geo_cylinder(top, Vector3Add(top, (Vector3){ 0, 0.35f, 0 }),
                         1.35f, 1.15f, 8, (Color){ 40, 42, 46, 255 }, 0.0f);
            Vector3 lp = Vector3Add(top, (Vector3){ 0, 0.75f, 0 });
            geo_box(lp, g->right, g->up, g->fwd, 0.55f, 0.45f, 0.55f,
                    (Color){ 255, 236, 190, 255 }, 1.0f);
            glow_add(lp, (Color){ 255, 234, 170, 255 }, 1.1f, 1.0f, 0.30f);
            // the sweep runs on the sim clock, so --seek stays deterministic
            float ang = W.clock*1.05f + lat_f(x->seed)*6.2832f;
            Vector3 dir = { cosf(ang), -0.18f, sinf(ang) };
            for (int j = 1; j <= 5; j++)
                glow_add(Vector3Add(lp, Vector3Scale(dir, 10.0f*(float)j)),
                         (Color){ 255, 240, 200, 255 }, 1.2f + 0.8f*(float)j, 0.6f,
                         0.10f*(1.0f - 0.12f*(float)j));
            break; }
        default: break;
        }
    }
}
