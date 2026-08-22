#pragma once
#include "raylib.h"

// Sign atlas panels.
enum { SA_EXIT_BIG, SA_EXIT_SMALL, SA_SPEED, SA_FUEL, SA_BILL1, SA_BILL2, SA_BILL3, SA_COUNT };

void  render_init(void);
void  render_frame(Camera3D cam, float sCam, float dt);   // call inside BeginDrawing
void  render_adas_time(float simTime);                    // drive ADAS phase from sim clock

// Immediate geometry, lit and fogged by the shared shader-less pipeline.
void  geo_quad(Vector3 a, Vector3 b, Vector3 c, Vector3 d, Color col, float emis);
void  geo_box(Vector3 c, Vector3 rx, Vector3 uy, Vector3 fz,
              float hx, float hy, float hz, Color col, float emis);
void  geo_cylinder(Vector3 a, Vector3 b, float ra, float rb, int sides, Color col, float emis);
void  geo_cone(Vector3 base, Vector3 axis, float r, float h, int sides, Color col, float emis);

void  glow_add(Vector3 p, Color c, float size, float yStretch, float alpha);
// Wet-road light streak: p sits just above the road surface; drawn only when
// the road is wet, as a tall additive smear below the light source.
void  refl_add(Vector3 surfaceP, Color c, float size, float yStretch, float alpha);
void  flatspot_add(Vector3 p, Vector3 right, Vector3 fwd, float hw, float hl,
                   Color c, float alpha);
void  sign_add(int atlas, Vector3 bl, Vector3 br, Vector3 tr, Vector3 tl);
