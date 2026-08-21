#pragma once
#include "raylib.h"

enum { VT_SEDAN, VT_SUV, VT_PICKUP, VT_TRUCK, VT_COUNT };

void vehicle_dims(int type, float *len, float *w, float *h);
void vehicle_draw(Vector3 pos, Vector3 fwd, Vector3 right, int type,
                  Color col, Color col2, float lights, float brake,
                  float wet, int oncoming, int blinker);
