#include "road.h"
#include "util.h"
#include "rendering.h"
#include "environment.h"
#include "weather.h"
#include <string.h>
#include <stdio.h>

typedef struct {
    int   type;
    float s0, s1;
    float tunnelS, tunnelLen;
    float bridgeS, bridgeLen;
    float gasS;
    float overS[3];
    int   overN;
    float gantryS;
    float constrS, constrLen;
} Zone;

static struct {
    uint64_t seed;
    Rng      zr;
    Vector3  P;
    float    yaw;
    int      head;          // next global seg index to generate
    float    cA[3], cW[3], cP[3];
    float    eA[3], eW[3], eP[3];
    Seg      ring[RING_SEGS];
    Zone     z[16];
    int      zn;
    int      prevType;
    int      firstZone;
} R;

static const float ZAMP [ZN_COUNT] = { 10.0f, 16.0f, 12.0f, 34.0f, 8.0f, 4.0f, 6.0f };
static const float ZCURV[ZN_COUNT] = { 0.70f, 1.00f, 0.90f, 1.50f, 1.20f, 0.35f, 0.90f };
static const float ZELEV[ZN_COUNT] = { 0.60f, 1.00f, 0.80f, 1.60f, 1.10f, 0.40f, 0.70f };
static const float ZLAKE[ZN_COUNT] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f };

static Zone *zone_at(float s){
    for (int i = 0; i < R.zn; i++)
        if (s >= R.z[i].s0 && s < R.z[i].s1) return &R.z[i];
    return R.zn ? &R.z[R.zn - 1] : NULL;
}

int road_zone_at(float s){
    Zone *z = zone_at(s);
    return z ? z->type : ZN_PLAINS;
}

// Zone parameter blended over the first 140 m of each zone.
static float zone_scale(float s, const float *tab){
    for (int i = 0; i < R.zn; i++){
        if (s >= R.z[i].s0 && s < R.z[i].s1){
            float prev = (i > 0) ? tab[R.z[i-1].type] : tab[R.prevType];
            float t = smooth01((s - R.z[i].s0) / 140.0f);
            return lerpf(prev, tab[R.z[i].type], t);
        }
    }
    return tab[ZN_PLAINS];
}

static void new_zone(float startS){
    if (R.zn >= 16) return;
    Zone *z = &R.z[R.zn];
    memset(z, 0, sizeof(*z));
    z->s0 = startS;
    Rng *r = &R.zr;

    int t = ZN_PLAINS;
    if (!R.firstZone){
        int prevTwice = (R.zn > 0 && R.z[R.zn-1].type == R.prevType);
        for (int tries = 0; tries < 8; tries++){
            int x = rng_int(r, 100);
            t = x < 20 ? ZN_PLAINS : x < 38 ? ZN_HILLS : x < 54 ? ZN_FOREST :
                x < 69 ? ZN_MOUNTAIN : x < 78 ? ZN_CANYON : x < 90 ? ZN_CITY : ZN_LAKESIDE;
            if (!prevTwice || t != R.prevType) break;
        }
    }
    R.firstZone = 0;
    z->type = t;
    static const float ZLEN[ZN_COUNT][2] = {
        {1400, 2600}, {1200, 2400}, {1200, 2200}, {1400, 2600}, {900, 1700}, {1600, 3000},
        {1100, 2200}
    };
    z->s1 = z->s0 + rng_range(r, ZLEN[t][0], ZLEN[t][1]);

    if (t == ZN_MOUNTAIN && rng_chance(r, 55)){
        z->tunnelS   = z->s0 + rng_range(r, 0.25f, 0.6f)*(z->s1 - z->s0);
        z->tunnelLen = rng_range(r, 180.0f, 420.0f);
    }
    if (t == ZN_CANYON && rng_chance(r, 80)){
        z->bridgeS   = z->s0 + rng_range(r, 0.3f, 0.6f)*(z->s1 - z->s0);
        z->bridgeLen = rng_range(r, 160.0f, 400.0f);
    }
    if (t == ZN_CITY){
        z->gantryS = z->s0 + 120.0f;
        z->overN   = 2 + rng_int(r, 2);
        float step = (z->s1 - z->s0 - 400.0f) / z->overN;
        for (int i = 0; i < z->overN; i++) z->overS[i] = z->s0 + 250.0f + step*i + rng_f(r)*120.0f;
    }
    if ((t == ZN_PLAINS || t == ZN_CITY || t == ZN_LAKESIDE) && rng_chance(r, 30)){
        // skip when leaving a city into a non-city zone: those approaches
        // already passed the last station
        if (t == ZN_CITY || R.zn == 0 || R.z[R.zn-1].type != ZN_CITY)
            z->gasS = z->s0 + 0.5f*(z->s1 - z->s0) + rng_range(r, -200.0f, 200.0f);
    }
    if (rng_chance(r, 8)){
        z->constrS   = z->s0 + rng_range(r, 0.15f, 0.75f)*(z->s1 - z->s0);
        z->constrLen = rng_range(r, 160.0f, 260.0f);
    }
    R.zn++;
}

void road_init(uint64_t seed){
    memset(&R, 0, sizeof(R));
    R.seed = seed;
    R.zr.s = mix_seed(seed, 0xA100);
    R.yaw = 0.0f;
    R.head = 0;
    R.firstZone = 1;
    R.prevType = ZN_HILLS;

    Rng r; r.s = mix_seed(seed, 0xC0DE);
    float cAmp[3]  = { 1.0f/2400.0f, 1.0f/1050.0f, 1.0f/520.0f };
    float cFreq[3] = { 0.0016f, 0.0043f, 0.0093f };
    float eAmp[3]  = { 4.5f, 1.8f, 0.9f };
    float eFreq[3] = { 0.0042f, 0.0113f, 0.0271f };
    for (int i = 0; i < 3; i++){
        R.cA[i] = cAmp[i]*rng_range(&r, 0.6f, 1.15f);
        R.cW[i] = cFreq[i]*rng_range(&r, 0.8f, 1.25f);
        R.cP[i] = rng_range(&r, 0.0f, 6.283f);
        R.eA[i] = eAmp[i]*rng_range(&r, 0.7f, 1.2f);
        R.eW[i] = eFreq[i]*rng_range(&r, 0.8f, 1.25f);
        R.eP[i] = rng_range(&r, 0.0f, 6.283f);
    }
    while (R.zn == 0 || R.z[R.zn-1].s1 < 4200.0f) new_zone(R.zn ? R.z[R.zn-1].s1 : 0.0f);
}

float road_curv_at(float s){
    float c = 0.0f;
    for (int i = 0; i < 3; i++) c += R.cA[i]*sinf(R.cW[i]*s + R.cP[i]);
    return c*zone_scale(s, ZCURV);
}

static float elev_at(float s){
    float e = 0.0f;
    for (int i = 0; i < 3; i++) e += R.eA[i]*sinf(R.eW[i]*s + R.eP[i]);
    return e*zone_scale(s, ZELEV);
}

Seg *road_seg(int gi){
    if (gi < R.head - RING_SEGS || gi >= R.head || gi < 0) return NULL;
    return &R.ring[gi & MASK];
}

int road_ring_head(void){ return R.head; }

void road_debug_zones(void){
    static const char *NAMES[ZN_COUNT] = { "plains", "hills", "forest", "mountain", "canyon", "city", "lakeside" };
    for (int i = 0; i < R.zn; i++){
        Zone *z = &R.z[i];
        printf("zone %-9s %6.0f - %6.0f m", NAMES[z->type], z->s0, z->s1);
        if (z->tunnelLen > 0.0f) printf("  tunnel %.0f+%.0f", z->tunnelS, z->tunnelLen);
        if (z->bridgeLen > 0.0f) printf("  bridge %.0f+%.0f", z->bridgeS, z->bridgeLen);
        if (z->gasS > 0.0f)      printf("  gas %.0f", z->gasS);
        if (z->constrLen > 0.0f) printf("  constr %.0f+%.0f", z->constrS, z->constrLen);
        if (z->type == ZN_CITY)  printf("  gantry %.0f overpasses=%d", z->gantryS, z->overN);
        printf("\n");
    }
}

static void gen_one(void){
    int i = R.head;
    float s  = (float)i*SEG_LEN;
    float sm = s + SEG_LEN*0.5f;

    float curv = road_curv_at(sm);
    R.yaw += curv*SEG_LEN;
    R.P.x += sinf(R.yaw)*SEG_LEN;
    R.P.z += cosf(R.yaw)*SEG_LEN;
    R.P.y = elev_at(sm);

    Seg *g = &R.ring[i & MASK];
    g->pos  = R.P;
    g->s    = s;
    g->seed = hash_u32((uint32_t)i*2654435761u ^ (uint32_t)(R.seed >> 32));
    Vector3 f = { sinf(R.yaw), 0.0f, cosf(R.yaw) };
    float bank = clampf(curv*34.0f, -0.10f, 0.10f);
    Vector3 rh = Vector3Normalize(Vector3CrossProduct(f, (Vector3){ 0.0f, 1.0f, 0.0f }));
    Vector3 rb = Vector3Normalize(Vector3Add(Vector3Scale(rh, cosf(bank)),
                                             (Vector3){ 0.0f, sinf(bank), 0.0f }));
    g->fwd   = f;
    g->right = rb;
    g->up    = Vector3Normalize(Vector3CrossProduct(rb, f));
    g->terraAmp = zone_scale(sm, ZAMP);
    g->lakeW = zone_scale(sm, ZLAKE);

    Zone *z = zone_at(s);
    uint8_t fl = 0;
    if (z){
        g->zone = (uint8_t)z->type;
        if (z->type == ZN_CITY) fl |= SEG_CITY;
        if (z->tunnelLen > 0.0f && s >= z->tunnelS && s < z->tunnelS + z->tunnelLen) fl |= SEG_TUNNEL;
        else if (z->bridgeLen > 0.0f && s >= z->bridgeS && s < z->bridgeS + z->bridgeLen) fl |= SEG_BRIDGE;
        if (z->constrLen > 0.0f && s >= z->constrS && s < z->constrS + z->constrLen) fl |= SEG_CONSTRUCTION;
        if (z->gasS > 0.0f && s >= z->gasS && s < z->gasS + 140.0f) fl |= SEG_GAS;
        for (int k = 0; k < z->overN; k++)
            if (s >= z->overS[k] - 6.0f && s < z->overS[k] + 6.0f) fl |= SEG_OVERPASS;
        if (z->gantryS > 0.0f && s >= z->gantryS - 8.0f && s < z->gantryS + 8.0f) fl |= SEG_GANTRY;
    } else {
        g->zone = ZN_PLAINS;
    }
    g->flags = fl;
    R.head++;
}

void road_update(float sCam){
    int camI = (int)floorf(sCam / SEG_LEN);
    // drop old zones first, extend the chain, then generate segments
    while (R.zn > 1 && R.z[0].s1 < sCam - 600.0f){
        R.prevType = R.z[0].type;
        R.zn--;
        memmove(R.z, R.z + 1, (size_t)R.zn*sizeof(Zone));
    }
    float cover = (float)(camI + DRAW_AHEAD_SEGS + 48)*SEG_LEN + 2400.0f;
    int guard = 0;
    while ((R.zn == 0 || R.z[R.zn-1].s1 < cover) && R.zn < 16 && guard++ < 64)
        new_zone(R.zn ? R.z[R.zn-1].s1 : 0.0f);
    while (R.head < camI + DRAW_AHEAD_SEGS + 48) gen_one();

    // tunnel ambience ramp
    uint8_t f = 0;
    Seg *a = road_seg(camI + 2);
    if (a) f = a->flags;
    float tgt = (f & SEG_TUNNEL) ? 1.0f : 0.0f;
    envIndoor += (tgt - envIndoor)*0.12f;
}

// shared ring lookup: clamp s to the generated window and locate the bracket
// segments; *pa is NULL only when the whole window is empty
static void ring_at(float s, Seg **pa, Seg **pb, float *t){
    if (s < 0.0f) s = 0.0f;
    float maxS = (float)(R.head - 2)*SEG_LEN;
    if (s > maxS) s = maxS;
    int i = (int)floorf(s / SEG_LEN);
    *t = s/SEG_LEN - (float)i;
    Seg *A = road_seg(i), *B = road_seg(i + 1);
    if (!A) A = B;
    *pa = A;
    *pb = (A && !B) ? A : B;
}

Vector3 road_point(float s, float lat, float h){
    Seg *A, *B; float t;
    ring_at(s, &A, &B, &t);
    if (!A) return (Vector3){ 0, 0, 0 };
    Vector3 p = Vector3Lerp(A->pos, B->pos, t);
    Vector3 r = Vector3Normalize(Vector3Lerp(A->right, B->right, t));
    Vector3 u = Vector3Normalize(Vector3Lerp(A->up, B->up, t));
    return Vector3Add(p, Vector3Add(Vector3Scale(r, lat), Vector3Scale(u, h)));
}

void road_frame(float s, Vector3 *pos, Vector3 *fwd, Vector3 *right, Vector3 *up){
    Seg *A, *B; float t;
    ring_at(s, &A, &B, &t);
    if (!A){ *pos = (Vector3){0,0,0}; *fwd = (Vector3){0,0,1}; *right = (Vector3){-1,0,0}; *up = (Vector3){0,1,0}; return; }
    *pos   = Vector3Lerp(A->pos, B->pos, t);
    *fwd   = Vector3Normalize(Vector3Lerp(A->fwd, B->fwd, t));
    *right = Vector3Normalize(Vector3Lerp(A->right, B->right, t));
    *up    = Vector3Normalize(Vector3Lerp(A->up, B->up, t));
}

void road_frame_at(float s, float lat, int dir,
                   Vector3 *pos, Vector3 *fwd, Vector3 *right, Vector3 *up){
    road_frame(s, pos, fwd, right, up);
    *pos = Vector3Add(*pos, Vector3Scale(*right, lat));
    if (dir < 0){
        *fwd = Vector3Scale(*fwd, -1.0f);
        *right = Vector3Scale(*right, -1.0f);
    }
}

uint8_t road_flags_at(float s){
    int i = (int)floorf(s / SEG_LEN);
    Seg *g = road_seg(i);
    return g ? g->flags : 0;
}

// ---------------- drawing ----------------

static inline Vector3 xpt(Seg *g, float lat, float h){
    return Vector3Add(g->pos, Vector3Add(Vector3Scale(g->right, lat), Vector3Scale(g->up, h)));
}

static const float PROWS[5] = {
    MEDIAN_HALF,
    MEDIAN_HALF + SHOULDER_L,
    MEDIAN_HALF + SHOULDER_L + LANE_W,
    MEDIAN_HALF + SHOULDER_L + 2.0f*LANE_W,
    PAVE_OUT
};

static void pavement_quad(Seg *A, Seg *B, float l0, float l1, float h, Color c, float emis){
    geo_quad(xpt(A, l0, h), xpt(A, l1, h), xpt(B, l1, h), xpt(B, l0, h), c, emis);
}

void road_draw(float sCam){
    int camI = (int)floorf(sCam / SEG_LEN);
    int i0 = camI - DRAW_BACK_SEGS;
    if (i0 < 0) i0 = 0;

    float wet = weather_wet();
    float snow = weather_ground_snow();
    Color asA = { 54, 56, 60, 255 };
    Color asS = { 46, 48, 52, 255 };
    Color asR = { 49, 51, 55, 255 };
    asA = col_lerp(col_scale(asA, 1.0f - 0.22f*wet), (Color){ 70, 80, 96, 255 }, 0.15f*wet);
    asS = col_scale(asS, 1.0f - 0.22f*wet);
    asR = col_lerp(col_scale(asR, 1.0f - 0.22f*wet), (Color){ 70, 80, 96, 255 }, 0.15f*wet);
    if (snow > 0.01f){
        asA = col_lerp(asA, (Color){ 176, 180, 190, 255 }, 0.20f*snow);
        asS = col_lerp(asS, (Color){ 200, 204, 214, 255 }, 0.55f*snow);
        asR = col_lerp(asR, (Color){ 200, 204, 214, 255 }, 0.55f*snow);
    }
    Color mWhite = col_scale((Color){ 228, 228, 222, 255 }, 1.0f - 0.45f*snow);
    Color mYell  = col_scale((Color){ 208, 168, 70, 255 }, 1.0f - 0.45f*snow);
    Color railC  = { 150, 152, 156, 255 };

    for (int i = i0; i <= camI + DRAW_AHEAD_SEGS; i++){
        Seg *A = road_seg(i), *B = road_seg(i + 1);
        if (!A || !B) break;
        float var = 0.92f + 0.16f*lat_f(A->seed);
        Color cA = col_scale(asA, var), cS = col_scale(asS, var), cR = col_scale(asR, var);
        uint8_t fl = A->flags;

        // our carriageway: shoulder, 2 lanes, shoulder
        pavement_quad(A, B, PROWS[0], PROWS[1], 0.0f, cS, 0.0f);
        pavement_quad(A, B, PROWS[1], PROWS[2], 0.0f, cA, 0.0f);
        pavement_quad(A, B, PROWS[2], PROWS[3], 0.0f, cA, 0.0f);
        pavement_quad(A, B, PROWS[3], PROWS[4], 0.0f, cR, 0.0f);
        // oncoming side, mirrored
        pavement_quad(A, B, -PROWS[4], -PROWS[3], 0.0f, cR, 0.0f);
        pavement_quad(A, B, -PROWS[3], -PROWS[2], 0.0f, cA, 0.0f);
        pavement_quad(A, B, -PROWS[2], -PROWS[1], 0.0f, cA, 0.0f);
        pavement_quad(A, B, -PROWS[1], -PROWS[0], 0.0f, cS, 0.0f);

        // median
        Color med = (A->zone == ZN_CITY) ? (Color){ 58, 57, 55, 255 }
                                         : (Color){ 52, 60, 38, 255 };
        med = col_lerp(med, (Color){ 190, 194, 202, 255 }, 0.5f*snow);
        pavement_quad(A, B, -MEDIAN_HALF, MEDIAN_HALF, -0.22f, med, 0.0f);

        int barrier = (fl & (SEG_CITY | SEG_TUNNEL | SEG_BRIDGE)) != 0;
        if (barrier){
            Color bc = { 168, 166, 160, 255 };
            Vector3 ax[3] = { A->right, A->up, A->fwd };
            geo_box(xpt(A,  3.82f, 0.58f), ax[0], ax[1], ax[2], 0.26f, 0.58f, SEG_LEN*0.51f, bc, 0.0f);
            geo_box(xpt(A, -3.82f, 0.58f), ax[0], ax[1], ax[2], 0.26f, 0.58f, SEG_LEN*0.51f, bc, 0.0f);
        }

        // guardrails on open highways
        if (!barrier){
            Vector3 ax[3] = { A->right, A->up, A->fwd };
            geo_box(xpt(A,  15.15f, 0.62f), ax[0], ax[1], ax[2], 0.035f, 0.16f, SEG_LEN*0.51f, railC, 0.0f);
            geo_box(xpt(A, -15.15f, 0.62f), ax[0], ax[1], ax[2], 0.035f, 0.16f, SEG_LEN*0.51f, railC, 0.0f);
            if ((i & 1) == 0){
                geo_box(xpt(A,  15.15f, 0.32f), ax[0], ax[1], ax[2], 0.05f, 0.32f, 0.05f, col_scale(railC, 0.7f), 0.0f);
                geo_box(xpt(A, -15.15f, 0.32f), ax[0], ax[1], ax[2], 0.05f, 0.32f, 0.05f, col_scale(railC, 0.7f), 0.0f);
            }
        }

        // lane markings, near range only
        if (i < camI + 96){
            float hm = 0.035f;
            float mE = 0.10f + 0.22f*envl.lightsOn;
            pavement_quad(A, B, 5.09f, 5.24f, hm, mYell, mE);
            pavement_quad(A, B, 14.48f, 14.62f, hm, mWhite, mE);
            pavement_quad(A, B, -5.24f, -5.09f, hm, mYell, mE);
            pavement_quad(A, B, -14.62f, -14.48f, hm, mWhite, mE);
            if ((i % 3) == 0){
                pavement_quad(A, B, 8.53f, 8.67f, hm, mWhite, mE);
                pavement_quad(A, B, -8.67f, -8.53f, hm, mWhite, mE);
            }
        }

        if (fl & SEG_TUNNEL){
            Color wc = { 66, 66, 70, 255 }, cc = { 54, 54, 58, 255 };
            // right wall (normal toward center)
            geo_quad(xpt(A, 16.0f, -0.5f), xpt(A, 16.0f, 6.6f), xpt(B, 16.0f, 6.6f), xpt(B, 16.0f, -0.5f), wc, 0.0f);
            // left wall
            geo_quad(xpt(A, -16.0f, 6.6f), xpt(A, -16.0f, -0.5f), xpt(B, -16.0f, -0.5f), xpt(B, -16.0f, 6.6f), wc, 0.0f);
            // ceiling
            pavement_quad(A, B, -16.0f, 16.0f, 6.6f, cc, 0.0f);
            if ((i % 6) == 0){
                Vector3 lp = xpt(A, 0.0f, 6.42f);
                Vector3 ax[3] = { A->right, A->up, A->fwd };
                geo_box(lp, ax[0], ax[1], ax[2], 0.8f, 0.06f, 0.35f, (Color){ 255, 236, 200, 255 }, 1.0f);
                glow_add(lp, (Color){ 255, 226, 170, 255 }, 1.1f, 1.0f, 0.16f);
                flatspot_add(xpt(A, 0.0f, 0.08f), A->right, A->fwd, 7.0f, 6.0f,
                             (Color){ 255, 220, 160, 255 }, 0.085f);
            }
        } else if (fl & SEG_BRIDGE){
            Vector3 ax[3] = { A->right, A->up, A->fwd };
            Color pc = { 150, 148, 144, 255 };
            geo_box(xpt(A,  14.85f, 0.55f), ax[0], ax[1], ax[2], 0.30f, 0.55f, SEG_LEN*0.51f, pc, 0.0f);
            geo_box(xpt(A, -14.85f, 0.55f), ax[0], ax[1], ax[2], 0.30f, 0.55f, SEG_LEN*0.51f, pc, 0.0f);
            if ((i % 8) == 0){
                Color pl = { 118, 116, 112, 255 };
                geo_box(xpt(A,  9.0f, -27.0f), ax[0], ax[1], ax[2], 0.95f, 26.0f, 0.95f, pl, 0.0f);
                geo_box(xpt(A, -9.0f, -27.0f), ax[0], ax[1], ax[2], 0.95f, 26.0f, 0.95f, pl, 0.0f);
            }
        }

        // tunnel portals: one face wall wherever the tunnel flag changes state
        {
            Seg *P = road_seg(i - 1);
            if (P && ((fl ^ P->flags) & SEG_TUNNEL)){
                Vector3 ax[3] = { A->right, A->up, A->fwd };
                Color pc = { 128, 126, 120, 255 };
                geo_box(xpt(A,  27.0f, 6.0f), ax[0], ax[1], ax[2], 18.0f, 7.0f, 1.1f, pc, 0.0f);
                geo_box(xpt(A, -27.0f, 6.0f), ax[0], ax[1], ax[2], 18.0f, 7.0f, 1.1f, pc, 0.0f);
                geo_box(xpt(A,   0.0f, 10.0f), ax[0], ax[1], ax[2], 45.0f, 3.2f, 1.1f, pc, 0.0f);
            }
        }

        // overpass crossing above
        if (fl & SEG_OVERPASS){
            Seg *P = road_seg(i - 1);
            if (P && !(P->flags & SEG_OVERPASS)){
                Vector3 ax[3] = { A->right, A->up, A->fwd };
                Color dc = { 96, 94, 92, 255 }, pl = { 110, 108, 104, 255 };
                geo_box(xpt(A, 0.0f, 8.3f), ax[0], ax[1], ax[2], 26.0f, 0.75f, 5.0f, dc, 0.0f);
                for (int k = -1; k <= 1; k++)
                    geo_box(xpt(A, 8.0f*k, 7.55f), ax[0], ax[1], ax[2], 0.45f, 0.45f, 5.0f, col_scale(dc, 0.85f), 0.0f);
                geo_box(xpt(A,  22.0f, 4.0f), ax[0], ax[1], ax[2], 0.95f, 4.0f, 0.95f, pl, 0.0f);
                geo_box(xpt(A, -22.0f, 4.0f), ax[0], ax[1], ax[2], 0.95f, 4.0f, 0.95f, pl, 0.0f);
                Color pp = { 160, 158, 154, 255 };
                geo_box(xpt(A,  25.4f, 9.35f), ax[0], ax[1], ax[2], 0.30f, 0.50f, 5.0f, pp, 0.0f);
                geo_box(xpt(A, -25.4f, 9.35f), ax[0], ax[1], ax[2], 0.30f, 0.50f, 5.0f, pp, 0.0f);
                if (envl.lightsOn > 0.1f){
                    glow_add(xpt(A, 12.0f, 9.6f), (Color){ 255, 200, 130, 255 }, 0.7f, 1.0f, 0.10f);
                    glow_add(xpt(A, -12.0f, 9.6f), (Color){ 255, 200, 130, 255 }, 0.7f, 1.0f, 0.10f);
                }
            }
        }

        // sign gantry
        if (fl & SEG_GANTRY){
            Seg *P = road_seg(i - 1);
            if (P && !(P->flags & SEG_GANTRY)){
                Vector3 ax[3] = { A->right, A->up, A->fwd };
                Color gc = { 120, 122, 126, 255 };
                geo_box(xpt(A,  15.8f, 3.4f), ax[0], ax[1], ax[2], 0.16f, 3.4f, 0.16f, gc, 0.0f);
                geo_box(xpt(A, -15.8f, 3.4f), ax[0], ax[1], ax[2], 0.16f, 3.4f, 0.16f, gc, 0.0f);
                geo_box(xpt(A,  0.0f, 6.55f), ax[0], ax[1], ax[2], 16.0f, 0.18f, 0.18f, gc, 0.0f);
                float hw = 1.9f, hh = 0.75f, latc = 8.6f, hb = 5.35f;
                Vector3 c = xpt(A, latc, hb);
                Vector3 br = Vector3Add(c, Vector3Scale(A->right, hw));
                Vector3 tl = Vector3Add(Vector3Add(c, Vector3Scale(A->right, -hw)), Vector3Scale(A->up, 2.0f*hh));
                sign_add(SA_EXIT_SMALL,
                         Vector3Add(tl, Vector3Scale(A->up, -2.0f*hh)),
                         br,
                         Vector3Add(br, Vector3Scale(A->up, 2.0f*hh)),
                         tl);
            }
        }
    }
}
