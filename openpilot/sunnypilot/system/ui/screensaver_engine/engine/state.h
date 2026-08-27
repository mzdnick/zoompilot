#pragma once
#include <stdbool.h>
#include <stddef.h>

// Full-state snapshot for exact resume. state_save writes every engine
// singleton plus the host values below to one file; state_load copies them
// back, so the world continues at the departed moment with no replay.
// On state_load failure the caller must run a full init.

// Host-side values that live outside the engine modules. The host
// (main.c on desktop, id_api.c on device) reads and writes these directly.
typedef struct {
    float simT;     // simulation clock: ADAS overlay, autosave timing
    float camBob;   // camera bob phase
} HostState;

HostState *state_host(void);
void       state_set_path(const char *path);   // tests only: keep runs off the real file
bool       state_save(void);
bool       state_load(void);
