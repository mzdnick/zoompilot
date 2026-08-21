#pragma once
// Shared math, rng, and color helpers.
#include "raylib.h"
#include "raymath.h"
#include <stdint.h>
#include <math.h>

typedef struct { uint64_t s; } Rng;

static inline uint64_t rng_u64(Rng *r){
    uint64_t x = r->s;
    x ^= x << 13; x ^= x >> 7; x ^= x << 17;
    r->s = x;
    return x;
}
static inline float rng_f(Rng *r){ return (float)((rng_u64(r) >> 40) & 0xFFFFFF) / 16777216.0f; }
static inline float rng_range(Rng *r, float a, float b){ return a + (b - a)*rng_f(r); }
static inline int rng_int(Rng *r, int n){ return n <= 0 ? 0 : (int)(rng_u64(r) % (uint64_t)n); }
static inline int rng_chance(Rng *r, int pct){ return rng_int(r, 100) < pct; }

static inline uint64_t mix_seed(uint64_t s, uint32_t salt){
    s += 0x9E3779B97F4A7C15ULL + (uint64_t)salt*0x2545F4914F6CDD1DULL;
    s = (s ^ (s >> 30)) * 0xBF58476D1CE4E5B9ULL;
    s = (s ^ (s >> 27)) * 0x94D049BB133111EBULL;
    return s ^ (s >> 31);
}
static inline uint32_t hash_u32(uint32_t x){
    x ^= x >> 16; x *= 0x7FEB352DU; x ^= x >> 15; x *= 0x846CA68BU; x ^= x >> 16;
    return x;
}
static inline float lat_f(uint32_t i){ return (float)(hash_u32(i) & 0xFFFF) / 65535.0f; }

static inline float vnoise1(float x){
    int i = (int)floorf(x);
    float f = x - (float)i;
    float u = f*f*(3.0f - 2.0f*f);
    return (lat_f((uint32_t)i)*(1.0f - u) + lat_f((uint32_t)i + 1u)*u)*2.0f - 1.0f;
}
static inline float fbm1(float x, int oct){
    float a = 0.5f, sum = 0.0f;
    for (int i = 0; i < oct; i++){ sum += a*vnoise1(x); x = x*2.17f + 13.7f; a *= 0.5f; }
    return sum;
}

static inline float clampf(float v, float lo, float hi){ return v < lo ? lo : (v > hi ? hi : v); }
static inline float lerpf(float a, float b, float t){ return a + (b - a)*t; }
static inline float smooth01(float t){ t = clampf(t, 0.0f, 1.0f); return t*t*(3.0f - 2.0f*t); }

static inline unsigned char cuc(int v){ return (unsigned char)(v < 0 ? 0 : (v > 255 ? 255 : v)); }
static inline Color col_lerp(Color a, Color b, float t){
    Color c;
    c.r = cuc(a.r + (int)((b.r - a.r)*t));
    c.g = cuc(a.g + (int)((b.g - a.g)*t));
    c.b = cuc(a.b + (int)((b.b - a.b)*t));
    c.a = 255;
    return c;
}
static inline Color col_scale(Color c, float k){
    k = clampf(k, 0.0f, 6.0f);
    Color r;
    r.r = cuc((int)(c.r*k)); r.g = cuc((int)(c.g*k)); r.b = cuc((int)(c.b*k)); r.a = c.a;
    return r;
}
static inline Color col_a(Color c, float a){
    c.a = cuc((int)(a*255.0f));
    return c;
}
