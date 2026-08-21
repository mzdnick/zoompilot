#include "traffic.h"
#include "road.h"
#include "vehicle.h"
#include "rendering.h"
#include "environment.h"
#include "weather.h"
#include "util.h"
#include <string.h>

#define NPC_MAX 56

typedef struct {
    int   active, dir, lane, targetLane;
    float s, lat, v, cruise, latOff;
    int   type;
    Color col, col2;
    int   changing;
    float changeT, fromLat, toLat;
    int   braking;
    float detect;
    Rng   rng;
    float decideT, laneTimer;
    int8_t signal;      // local side: -1 left, +1 right
    float  signalT;
} Npc;

static struct {
    Rng   rng;
    Npc   np[NPC_MAX];
    float s, v, lat;
    int   lane, targetLane;
    float decideT;
    float boxT;
    float wave, waveTarget, waveT;
    float simT;
    int   lastLaneChangeSign;
    int   showcase;
} T = {
    .rng = { 1 },
    .s = 260.0f, .v = 26.0f, .lat = LANE_SLOW,
    .lane = 1, .targetLane = 1,
    .decideT = 22.0f,
    .wave = 1.0f, .waveTarget = 1.0f, .waveT = 40.0f,
};

void traffic_set_showcase(int on){
    T.showcase = on;
    if (!on) return;
    T.v = 0.0f;
    static const int types[4] = { VT_SEDAN, VT_SUV, VT_PICKUP, VT_TRUCK };
    static const float zs[4] = { 18.0f, 27.0f, 38.0f, 54.0f };
    static const unsigned char cols[4][3] = {
        { 240, 240, 242 }, { 84, 88, 94 }, { 70, 30, 32 }, { 148, 150, 154 }
    };
    for (int k = 0; k < NPC_MAX; k++) T.np[k].active = 0;
    for (int k = 0; k < 4; k++){
        Npc *n = &T.np[k];
        memset(n, 0, sizeof(*n));
        n->active = 1;
        n->dir = 1;
        n->lane = n->targetLane = 1;
        n->type = types[k];
        n->s = T.s + zs[k];
        n->lat = LANE_SLOW;
        n->v = 0.0f;
        n->cruise = 0.0f;
        n->col = (Color){ cols[k][0], cols[k][1], cols[k][2], 255 };
        n->col2 = (Color){ 200, 202, 206, 255 };
        n->detect = 1.0f;
    }
}

float traffic_ego_s(void){ return T.s; }
float traffic_ego_v(void){ return T.v; }
float traffic_ego_lat(void){ return T.lat; }
int   traffic_ego_target_lane(void){ return T.targetLane; }

static float lane_lat(int dir, int lane){
    if (dir > 0) return lane == 1 ? LANE_SLOW : LANE_FAST;
    return lane == 1 ? -LANE_SLOW : -LANE_FAST;
}

int traffic_ego_blinker(void){
    float latT = lane_lat(1, T.targetLane);
    if (fabsf(latT - T.lat) < 0.35f) return 0;
    return (latT < T.lat) ? -1 : 1;   // toward lane 0 is the left side
}

static inline float vminf(float a, float b){ return a < b ? a : b; }
static inline float vmaxf(float a, float b){ return a > b ? a : b; }

static const unsigned char PAL[][3] = {
    { 240, 240, 242 }, { 168, 172, 178 }, { 84, 88, 94 }, { 36, 40, 46 },
    { 28, 44, 72 }, { 70, 30, 32 }, { 24, 26, 30 }, { 148, 150, 154 },
};
#define NPAL 8

static void npc_spawn(int i, float atS){
    Npc *n = &T.np[i];
    memset(n, 0, sizeof(*n));
    // step the spawn point forward until it clears the ego and every NPC;
    // a stacked spawn would drive in lockstep forever (leader_gap blind spot)
    for (int t = 0; t < 30; t++){
        int hit = (fabsf(atS - T.s) < 22.0f);
        for (int j = 0; j < NPC_MAX && !hit; j++)
            if (j != i && T.np[j].active && fabsf(T.np[j].s - atS) < 16.0f) hit = 1;
        if (!hit) break;
        atS += 20.0f;
    }
    n->active = 1;
    n->dir = (rng_int(&T.rng, 100) < 68) ? 1 : -1;
    n->lane = n->targetLane = rng_int(&T.rng, 100) < 58 ? 1 : 0;
    n->s = atS;
    n->lat = lane_lat(n->dir, n->lane);
    n->latOff = rng_range(&T.rng, -0.35f, 0.35f);
    n->rng.s = mix_seed(T.rng.s ^ (uint64_t)i, 0xFEED);
    int x = rng_int(&T.rng, 100);
    if (n->dir < 0 && x < 20) n->type = VT_TRUCK;
    else n->type = x < 45 ? VT_SEDAN : x < 70 ? VT_SUV : x < 85 ? VT_PICKUP : VT_TRUCK;
    float base = (n->lane == 0) ? rng_range(&n->rng, 26.0f, 31.0f) : rng_range(&n->rng, 21.0f, 30.0f);
    if (n->type == VT_TRUCK) base -= 4.0f;
    n->cruise = base;
    n->v = base;
    n->decideT = rng_range(&n->rng, 0.2f, 1.2f);
    int ci = rng_int(&T.rng, NPAL);
    n->col = (Color){ PAL[ci][0], PAL[ci][1], PAL[ci][2], 255 };
    int c2 = rng_int(&T.rng, 4);
    n->col2 = c2 == 0 ? (Color){ 200, 202, 206, 255 } :
              c2 == 1 ? (Color){ 170, 178, 188, 255 } :
              c2 == 2 ? (Color){ 60, 80, 118, 255 } : (Color){ 120, 118, 114, 255 };
}

void traffic_init(uint64_t seed){
    memset(&T, 0, sizeof(T));
    T.rng.s = mix_seed(seed, 0x7E55);
    T.s = 260.0f; T.v = 26.0f; T.lat = LANE_SLOW;
    T.lane = 1; T.targetLane = 1;
    T.decideT = 22.0f; T.wave = 1.0f; T.waveTarget = 1.0f; T.waveT = 40.0f;
    for (int i = 0; i < 26; i++) npc_spawn(i, T.s + rng_range(&T.rng, -60.0f, 600.0f));
}

// leader search helper: nearest vehicle ahead of s in given lane
static float leader_gap(float s, int dir, int lane, float *outV, int *outIdx){
    float best = 1e9f;
    *outV = 40.0f; *outIdx = -1;
    for (int i = 0; i < NPC_MAX; i++){
        Npc *n = &T.np[i];
        if (!n->active || n->dir != dir) continue;
        int sameLane = (n->targetLane == lane) || (n->lane == lane);
        if (!sameLane) continue;
        float len;
        { float l, w, h; vehicle_dims(n->type, &l, &w, &h); len = l; }
        float d = (n->s - s)*dir - len*0.5f;
        if (d >= -1.0f && d < best){ best = d; *outV = n->v; *outIdx = i; }
    }
    return best;
}

// directional gap check: fixed window ahead, speed-scaled window behind so a
// merge never rear-ends an approaching follower; the ego (not in np[]) blocks
// NPC merges too
static int lane_clear2(float s, int dir, int lane, float back, float fwd, float vSelf){
    for (int i = 0; i < NPC_MAX; i++){
        Npc *n = &T.np[i];
        if (!n->active || n->dir != dir) continue;
        if (n->targetLane != lane && n->lane != lane) continue;
        float d = (n->s - s)*dir;
        float need = back;
        if (d < 0.0f && n->v > vSelf) need = vmaxf(need, 8.0f + 2.6f*(n->v - vSelf));
        if (d > -need && d < fwd) return 0;
    }
    if (dir > 0){
        int egoMid = fabsf(T.lat - lane_lat(1, T.targetLane)) > 1.1f;
        if (T.targetLane == lane || egoMid){
            float d = T.s - s;
            float need = back;
            if (d < 0.0f && T.v > vSelf) need = vmaxf(need, 8.0f + 2.6f*(T.v - vSelf));
            if (d > -need && d < fwd) return 0;
        }
    }
    return 1;
}

void traffic_update(float dt){
    T.simT += dt;
    if (T.showcase){
        // parked QA row: hold everything still
        T.v = 0.0f;
        for (int i = 0; i < NPC_MAX; i++)
            if (T.np[i].active) T.np[i].detect = 1.0f;
        return;
    }

    // slow density waves
    T.waveT -= dt;
    if (T.waveT <= 0.0f){
        T.waveTarget = rng_range(&T.rng, 0.75f, 1.55f);
        T.waveT = rng_range(&T.rng, 120.0f, 300.0f);
    }
    T.wave += (T.waveTarget - T.wave)*clampf(dt/60.0f, 0.0f, 1.0f);

    int zone = road_zone_at(T.s);
    static const float ZDENS[ZN_COUNT] = { 14, 16, 16, 18, 15, 34 };
    int target = (int)(ZDENS[zone]*T.wave);
    if (target > NPC_MAX) target = NPC_MAX;

    // ---------- ego ----------
    uint8_t aheadFlags = road_flags_at(T.s + 150.0f);
    int constrAhead = (aheadFlags & SEG_CONSTRUCTION) != 0;
    float vtFree = 32.0f;
    if (zone == ZN_CITY) vtFree *= 0.78f;
    float curvMax = 0.0f;
    for (int k = 0; k < 5; k++){
        float c = fabsf(road_curv_at(T.s + 20.0f*k));
        if (c > curvMax) curvMax = c;
    }
    float vmax = 3.2f/sqrtf(curvMax + 0.0004f);
    if (vmax > 34.0f) vmax = 34.0f;
    if (vmax < vtFree) vtFree = vmax;
    float vt = vtFree;
    if (constrAhead) vt = vminf(vt, 20.0f);

    float lv;
    int li;
    float gap = leader_gap(T.s, 1, T.targetLane, &lv, &li);
    if (li >= 0){
        if (gap < 35.0f) vt = vminf(vt, lv + (gap - 16.0f)*0.22f);
        if (gap < 8.0f)  vt = vminf(vt, lv - 3.0f);
    }
    T.v += clampf(vt - T.v, -3.0f*dt, 2.2f*dt);
    if (T.v < 12.0f) T.v = 12.0f;
    T.s += T.v*dt;

    // ego lane choice: judged against the free cruise so settling behind a
    // slow leader never disarms the pass decision
    int slowLeader = (li >= 0 && gap < 90.0f && lv < vtFree - 1.5f);
    if (slowLeader && T.targetLane == 1) T.boxT += dt; else T.boxT = 0.0f;
    // boxed in: after 6 s of waiting accept a tighter merge gap
    float backPass = (T.boxT > 6.0f) ? 28.0f : 45.0f;
    T.decideT -= dt;
    int wantLane = T.targetLane;
    if (constrAhead || (road_flags_at(T.s + 60.0f) & SEG_CONSTRUCTION)) wantLane = 0;
    else if (T.decideT <= 0.0f){
        if (slowLeader && T.targetLane == 1){
            T.decideT = rng_range(&T.rng, 1.5f, 3.5f);
            if (lane_clear2(T.s, 1, 0, backPass, 20.0f, T.v)) wantLane = 0;
        } else {
            T.decideT = rng_range(&T.rng, 4.0f, 9.0f);
            if (T.targetLane == 0 && lane_clear2(T.s, 1, 1, 50.0f, 25.0f, T.v))
                wantLane = 1;
        }
    }
    if (wantLane != T.targetLane){
        int ok = (wantLane == 0) ? lane_clear2(T.s, 1, 0, backPass, 20.0f, T.v)
                                 : lane_clear2(T.s, 1, 1, 50.0f, 25.0f, T.v);
        if (ok) T.targetLane = wantLane;
    }
    float latT = lane_lat(1, T.targetLane);
    T.lat += (latT - T.lat)*clampf(dt*1.05f, 0.0f, 1.0f);

    // ---------- NPCs ----------
    int active = 0;
    for (int i = 0; i < NPC_MAX; i++){
        Npc *n = &T.np[i];
        if (!n->active) continue;
        active++;
        if (n->signalT > 0.0f) n->signalT -= dt;

        // recycle
        if (n->dir > 0){
            if (n->s < T.s - 130.0f || n->s > T.s + 830.0f){
                if (active <= target) npc_spawn(i, T.s + rng_range(&T.rng, 620.0f, 800.0f));
                else { n->active = 0; active--; }
                continue;
            }
        } else {
            if (n->s < T.s - 90.0f){
                if (active <= target) npc_spawn(i, T.s + rng_range(&T.rng, 720.0f, 860.0f));
                else { n->active = 0; active--; }
                continue;
            }
        }

        // lane change progress
        if (n->changing){
            n->changeT += dt;
            float u = smooth01(n->changeT/2.6f);
            n->lat = lerpf(n->fromLat, n->toLat, u);
            if (n->changeT >= 2.6f){ n->changing = 0; n->lat = n->toLat; n->lane = n->targetLane; }
        }

        // speed control
        float nvt = n->cruise;
        uint8_t nfl = road_flags_at(n->s + 60.0f);
        if (n->dir > 0 && (nfl & SEG_CONSTRUCTION) && n->targetLane == 1)
            nvt = vminf(nvt, 21.0f);
        float lv2; int li2;
        float g2 = leader_gap(n->s, n->dir, n->targetLane, &lv2, &li2);
        // ego counts as a leader for same-direction NPCs
        if (n->dir > 0 && T.targetLane == n->targetLane){
            float ge = T.s - n->s;
            if (ge >= -1.0f && ge < g2){ g2 = ge; lv2 = T.v; }
        }
        int braking = 0;
        if (li2 >= 0 || (n->dir > 0 && T.targetLane == n->targetLane && T.s > n->s)){
            if (g2 < 45.0f){ nvt = vminf(nvt, lv2 + (g2 - 18.0f)*0.20f); }
            if (g2 < 8.0f){ nvt = vminf(nvt, lv2 - 4.0f); braking = 1; }
            if (nvt < n->v - 0.8f) braking = 1;
        }
        n->braking = braking;
        n->v += clampf(nvt - n->v, -3.4f*dt, 1.5f*dt);
        if (n->v < 8.0f) n->v = 8.0f;
        n->s += (n->dir > 0 ? n->v : -n->v)*dt;

        // decisions
        n->decideT -= dt;
        if (n->decideT <= 0.0f && !n->changing){
            n->decideT = rng_range(&n->rng, 0.6f, 2.2f);
            n->laneTimer += n->decideT;
            if (n->dir > 0){
                int blocked = (li2 >= 0 && g2 < 45.0f && lv2 < n->cruise - 1.5f);
                if (blocked && n->targetLane == 1 && lane_clear2(n->s, 1, 0, 38.0f, 12.0f, n->v)){
                    n->changing = 1; n->changeT = 0;
                    n->fromLat = n->lat; n->toLat = lane_lat(1, 0); n->targetLane = 0;
                    n->signal = -1; n->signalT = 3.4f;
                } else if (n->targetLane == 0 && n->laneTimer > 11.0f && lane_clear2(n->s, 1, 1, 40.0f, 15.0f, n->v)){
                    n->changing = 1; n->changeT = 0;
                    n->fromLat = n->lat; n->toLat = lane_lat(1, 1); n->targetLane = 1;
                    n->laneTimer = 0.0f;
                    n->signal = 1; n->signalT = 3.4f;
                }
            } else {
                // oncoming mirror: their fast lane is index 0
                int blocked = (li2 >= 0 && g2 < 45.0f && lv2 < n->cruise - 1.5f);
                if (blocked && n->targetLane == 1 && lane_clear2(n->s, -1, 0, 38.0f, 12.0f, n->v)){
                    n->changing = 1; n->changeT = 0;
                    n->fromLat = n->lat; n->toLat = lane_lat(-1, 0); n->targetLane = 0;
                    n->signal = -1; n->signalT = 3.4f;
                } else if (n->targetLane == 0 && n->laneTimer > 11.0f && lane_clear2(n->s, -1, 1, 40.0f, 15.0f, n->v)){
                    n->changing = 1; n->changeT = 0;
                    n->fromLat = n->lat; n->toLat = lane_lat(-1, 1); n->targetLane = 1;
                    n->laneTimer = 0.0f;
                    n->signal = 1; n->signalT = 3.4f;
                }
            }
        }

        // per-vehicle micro offset
        if (!n->changing)
            n->lat = lane_lat(n->dir, n->targetLane) + n->latOff*0.6f;

        // ADAS detect fade
        float da = n->s - T.s;
        float det = 0.0f;
        if (n->dir > 0 && da > -12.0f && da < 175.0f) det = 1.0f;
        if (n->dir < 0 && da > -10.0f && da < 125.0f) det = 1.0f;
        float rate = det > n->detect ? 2.2f : 1.1f;
        n->detect += (det - n->detect)*clampf(dt*rate, 0.0f, 1.0f);
    }

    // top up if under target
    if (active < target){
        for (int i = 0; i < NPC_MAX && active < target; i++){
            if (!T.np[i].active){
                npc_spawn(i, T.s + rng_range(&T.rng, 300.0f, 780.0f));
                active++;
            }
        }
    }
}

// ego blinker: amber corner glow plus spill on the road, in the cam's own
// driver-eye view (the ego body itself is not drawn)
static void ego_signal_lights(void){
    int bl = traffic_ego_blinker();
    if (!bl) return;
    if (fmodf(T.simT*1.4f, 1.0f) >= 0.55f) return;
    Vector3 pos, fwd, right, up;
    road_frame(T.s, &pos, &fwd, &right, &up);
    float side = (bl < 0) ? -1.0f : 1.0f;
    Vector3 c = Vector3Add(Vector3Add(pos, Vector3Scale(right, T.lat + side*0.92f)),
                           Vector3Scale(fwd, 2.30f));
    Color amber = { 255, 178, 48, 255 };
    glow_add((Vector3){ c.x, c.y + 0.72f, c.z }, amber, 0.24f, 1.0f, 0.40f);
    flatspot_add((Vector3){ c.x, c.y + 0.02f, c.z }, right, fwd, 0.95f, 1.45f, amber, 0.14f);
}

void traffic_draw(void){
    float lights = envl.lightsOn;
    float wet = weather_wet();
    ego_signal_lights();
    for (int i = 0; i < NPC_MAX; i++){
        Npc *n = &T.np[i];
        if (!n->active) continue;
        if (n->s < T.s - 30.0f || n->s > T.s + 820.0f) continue;
        Vector3 pos, fwd, right, up;
        road_frame(n->s, &pos, &fwd, &right, &up);
        pos = Vector3Add(pos, Vector3Scale(right, n->lat));
        if (n->dir < 0){ fwd = Vector3Scale(fwd, -1.0f); right = Vector3Scale(right, -1.0f); }
        int bl = 0;
        if (n->signalT > 0.0f && fmodf(T.simT*1.4f + n->s*0.31f, 1.0f) < 0.55f)
            bl = n->signal;
        vehicle_draw(pos, fwd, right, n->type, n->col, n->col2, lights,
                     n->braking ? 1.0f : 0.0f, wet, n->dir < 0, bl);
    }
}

int traffic_query(VehInfo *out, int max){
    int c = 0;
    for (int i = 0; i < NPC_MAX && c < max; i++){
        Npc *n = &T.np[i];
        if (!n->active || n->detect <= 0.02f) continue;
        VehInfo *v = &out[c++];
        Vector3 pos, fwd, right, up;
        road_frame(n->s, &pos, &fwd, &right, &up);
        v->pos = Vector3Add(pos, Vector3Scale(right, n->lat));
        v->fwd = n->dir > 0 ? fwd : Vector3Scale(fwd, -1.0f);
        v->right = n->dir > 0 ? right : Vector3Scale(right, -1.0f);
        v->s = n->s; v->lat = n->lat; v->v = n->v;
        vehicle_dims(n->type, &v->len, &v->w, &v->h);
        v->dir = n->dir; v->lane = n->targetLane; v->type = n->type;
        v->braking = n->braking; v->detect = n->detect;
        v->distAhead = n->s - T.s;
    }
    return c;
}
