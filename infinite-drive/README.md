# Infinite Drive

A procedural 3D screensaver in C with raylib. A camera drives itself down an
endless highway at night. No menus. No gameplay. It starts driving at once.

An autonomous-driving visualization layer is drawn on top: lane boundaries,
a planned path, bounding boxes, and lead-vehicle tracking. The layer fades in
and out so it never clutters the screen.

## Features

- Infinite procedural highway: smooth curves, hills, 2 lanes per direction.
- Divided road with median, shoulders, guardrails, and lane markings.
- Traffic in both directions. Vehicles keep lanes, vary speed, and overtake.
- Terrain zones: plains, hills, forest, mountain, canyon, city.
- Events: tunnels, bridges, overpasses, sign gantries, gas stations,
  road construction, heavier traffic waves.
- Full day cycle: night, dawn, morning, noon, golden hour, sunset.
- Weather: clear, rain, fog, snow. Transitions are gradual.
- Night lighting: headlight cone, glowing taillights, streetlights,
  lit building windows, distant city glow, stars and moon.
- ADAS overlay: path, lane edges, vehicle boxes, LEAD label, speed readout.
- Deterministic world from a seed. All pools are fixed size. No memory growth.

## Build

Needs a C compiler, `make`, and `git`. On macOS also Xcode command line tools.

    make

The first build clones raylib 6.0 into `third_party/` and builds it as a
static library. If `pkg-config` finds a system raylib, that one is used
instead.

Run it:

    make run

Or build with CMake:

    cmake -B build-cmake && cmake --build build-cmake
    ./build-cmake/infinite-drive

## Command line flags

    --seed N          world seed (default: from the clock)
    --res WxH         window size (default 1280x720)
    --fullscreen      start fullscreen
    --day-length S    seconds per full day cycle (default 480)
    --start-time T    start time of day, 0..1 (default 0.93, late night)
    --weather NAME    force weather: clear, rain, fog, snow (held forever)

Testing flags:

    --seek S          fast-forward S seconds of simulation first
    --shot PATH       save one screenshot, then exit
    --scan PREFIX     save screenshots at 35, 120, 300, 600, 960, 1440 s
    --bench N         run N frames without vsync, print average frame time
    --zones           print the zone layout (type, range, features) and exit

Keys: `ESC` quit, `F11` fullscreen, `F3` FPS counter.

## Performance

All geometry is drawn through one immediate-mode batch with per-vertex
lighting and fog on the CPU. Typical load is about 20k triangles per frame.

Measured on an Apple Silicon desktop at 1920x1080: about 3.5 ms per frame,
about 280 fps, with vsync off.

## Layout

    src/main.c         flags, main loop, camera
    src/road.c         centerline, segments, zones, road surface, tunnels,
                       bridges, overpasses, gantries
    src/world.c        terrain ribbon, scenery pool, buildings, gas stations,
                       construction
    src/traffic.c      ego vehicle and NPC simulation, lane changes, recycling
    src/vehicle.c      low-poly car geometry and lights
    src/environment.c  day cycle, sky, stars, sun, moon
    src/weather.c      weather states, rain and snow particles
    src/rendering.c    draw pipeline, lighting and fog, ADAS overlay
    src/ui.c           intro fade, title, vignette, FPS

World state lives in fixed-size ring buffers and pools. Objects behind the
camera are recycled. The same seed gives the same world every time.

## License

MIT, same as the repository.
