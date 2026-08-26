#pragma once
#include "raylib.h"
#include <stdint.h>

// Rare, seeded moments: shooting stars, truck convoys, classic-car runs,
// deer crossings, lightning storms, police traffic stops, city fireworks.
// All state advances on simulation time only.
void events_init(uint64_t seed);
void events_update(float dt);
void events_draw(void);                        // call inside the 3D triangle batch
void events_draw2d(void);                      // screen-space flash veil, after weather
int  events_star(Vector3 *dir, float *phase);   // 1 while a shooting star is live
