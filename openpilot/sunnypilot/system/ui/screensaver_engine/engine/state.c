// Full-state snapshot: every engine singleton is plain fixed-size data
// (no pointers, no heap), so the whole world serializes with memcpy and
// restores bit-identical. The file is written and read by the same engine
// build on the same machine; entry sizes double as a layout check, so an
// engine update with changed structs simply rejects the old file and the
// caller starts fresh.
#include "state.h"
#include "road.h"
#include "world.h"
#include "traffic.h"
#include "environment.h"
#include "weather.h"
#include "events.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ID_STATE_VERSION 1u
#define STATE_BUF_MAX   (512*1024)

static const uint8_t ID_STATE_MAGIC[8] = { 'I','D','S','T','A','T','E','\1' };

enum { ST_HOST = 1, ST_ROAD, ST_WORLD, ST_TRAFFIC, ST_ENV, ST_WEATHER, ST_EVENTS };

typedef struct {
    uint32_t tag;
    size_t   (*size)(void);
    void     (*save)(void *dst);
    void     (*load)(const void *src);
} StateMod;

static HostState hostS;

static size_t host_size(void){ return sizeof hostS; }
static void   host_save(void *dst){ memcpy(dst, &hostS, sizeof hostS); }
static void   host_load(const void *src){ memcpy(&hostS, src, sizeof hostS); }

static const StateMod mods[] = {
    { ST_HOST,    host_size,              host_save,              host_load },
    { ST_ROAD,    road_state_size,        road_state_save,        road_state_load },
    { ST_WORLD,   world_state_size,       world_state_save,       world_state_load },
    { ST_TRAFFIC, traffic_state_size,     traffic_state_save,     traffic_state_load },
    { ST_ENV,     environment_state_size, environment_state_save, environment_state_load },
    { ST_WEATHER, weather_state_size,     weather_state_save,     weather_state_load },
    { ST_EVENTS,  events_state_size,      events_state_save,      events_state_load },
};
#define MOD_N ((int)(sizeof mods / sizeof mods[0]))

// file layout: magic[8] u32 version u32 count, then per module
// u32 tag u32 size + payload, then u64 fnv1a over all bytes before it
static uint8_t buf[STATE_BUF_MAX];
static char statePath[512];

static void default_path(void){
    const char *home = getenv("HOME");
    snprintf(statePath, sizeof statePath, "%s/.infinite-drive.state", home ? home : ".");
}

HostState *state_host(void){ return &hostS; }

void state_set_path(const char *path){
    snprintf(statePath, sizeof statePath, "%s", path);
}

static uint64_t fnv1a(const uint8_t *p, size_t n){
    uint64_t h = 14695981039346656037ULL;
    for (size_t i = 0; i < n; i++){
        h ^= p[i];
        h *= 1099511628211ULL;
    }
    return h;
}

bool state_save(void){
    if (!statePath[0]) default_path();
    size_t off = 0;
    uint32_t ver = ID_STATE_VERSION, cnt = (uint32_t)MOD_N;
    memcpy(buf + off, ID_STATE_MAGIC, 8); off += 8;
    memcpy(buf + off, &ver, 4); off += 4;
    memcpy(buf + off, &cnt, 4); off += 4;
    for (int k = 0; k < MOD_N; k++){
        size_t sz = mods[k].size();
        if (off + 8 + sz + 8 > STATE_BUF_MAX) return false;
        uint32_t tag = mods[k].tag, sz32 = (uint32_t)sz;
        memcpy(buf + off, &tag, 4); off += 4;
        memcpy(buf + off, &sz32, 4); off += 4;
        mods[k].save(buf + off);
        off += sz;
    }
    uint64_t sum = fnv1a(buf, off);
    memcpy(buf + off, &sum, 8); off += 8;

    // write through a temp name so a power cut cannot leave a half file
    char tmp[540];
    snprintf(tmp, sizeof tmp, "%s.tmp", statePath);
    FILE *f = fopen(tmp, "wb");
    if (!f) return false;
    if (fwrite(buf, 1, off, f) != off || fclose(f) != 0) return false;
    return rename(tmp, statePath) == 0;
}

bool state_load(void){
    if (!statePath[0]) default_path();
    FILE *f = fopen(statePath, "rb");
    if (!f) return false;
    size_t n = fread(buf, 1, STATE_BUF_MAX, f);
    int trailing = fgetc(f);
    fclose(f);
    if (trailing != EOF || n < 24) return false;
    if (memcmp(buf, ID_STATE_MAGIC, 8) != 0) return false;
    uint32_t ver, cnt;
    memcpy(&ver, buf + 8, 4);
    memcpy(&cnt, buf + 12, 4);
    if (ver != ID_STATE_VERSION || cnt != (uint32_t)MOD_N) return false;
    uint64_t sum;
    memcpy(&sum, buf + n - 8, 8);
    if (fnv1a(buf, n - 8) != sum) return false;

    // entries may only be applied when the whole file checked out; a reject
    // here leaves partially copied state, which is fine because the caller
    // must fully re-init on false
    size_t off = 16;
    for (int k = 0; k < MOD_N; k++){
        if (off + 8 > n - 8) return false;
        uint32_t tag, sz;
        memcpy(&tag, buf + off, 4);
        memcpy(&sz, buf + off + 4, 4);
        off += 8;
        if (tag != mods[k].tag || sz != (uint32_t)mods[k].size()) return false;
        if (off + sz > n - 8) return false;
        mods[k].load(buf + off);
        off += sz;
    }
    return off + 8 == n;
}
