#pragma once

// persist: 0 in test runs (--seek/--shot/--bench/...), 1 in normal play.
// Only a persistent run loads and saves the trip odometer file.
void ui_init(int persist);
void ui_update(float dt);
void ui_draw(int w, int h);
void ui_fps_toggle(void);
void ui_shutdown(void);   // saves the odometer when persisting
