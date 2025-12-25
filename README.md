# we-engine (single-header Win32 2D engine)
A simple Win32 + Built-In code 2D game engine for creating fun little game projects.

A tiny-but-capable **2D game engine** written in **C++17** for Windows, built on **Win32** and a **manual software renderer** (pixel backbuffer).  
No SDL/SFML/GLFW. No external dependencies. Just Windows system libraries + your code.

This project is designed to be:
- **Fun to hack on**
- **Simple to use from `main.cpp`**
- **“Scary math” inside the engine** (matrices, easing, sampling, chunking, ECS…), while keeping the game code readable.

---

## Features

### Core / Platform
- ✅ **Win32 window + message loop**
- ✅ **High-resolution timing** (`QueryPerformanceCounter`)
- ✅ **Input system** with clean edge detection:
  - `key[]`, `key_pressed[]`, `key_released[]`
  - Mouse buttons + pressed/released
  - Mouse wheel + mouse delta

### Rendering (manual software renderer)
- ✅ **32-bit backbuffer** (`AARRGGBB`) rendered via `StretchDIBits`
- ✅ **Primitives:**
  - `rect_fill`, `rect_outline`
  - `line`
  - `circle_fill`
- ✅ **Alpha blending** (fast `blend_over`)
- ✅ **Clipping support** (UI uses this heavily)

### Text / Font
- ✅ **Built-in 5x7 bitmap font**
- ✅ Fixed spacing + line spacing
- ✅ **Supports:** letters, numbers, punctuation, newline, tab

### Images / Textures
- ✅ **Image loading via WIC** (Windows Imaging Component)
- ✅ **Works with common formats:** PNG, JPG/JPEG, BMP
- ✅ **Sprite blitting:**
  - Nearest OR bilinear sampling
  - Optional tint (multiply)
  - Alpha blend on/off

### Camera2D (stable zoom)
- ✅ Camera transform based on **proper 2D affine matrices**
- ✅ Correct `screen_to_world()` via **inverse matrix**
- ✅ Smooth follow + mouse-to-world conversion

### Tile World
- ✅ **Chunked tile storage** (32×32 per chunk)
- ✅ Procedural terrain generator
- ✅ Tile placement / digging demo logic included
- ✅ Optional tileset atlas rendering (16×16 tiles per cell)

### Physics
- ✅ AABB entity collision vs solid tiles
- ✅ Gravity + jump + ground detection
- ✅ Axis-separated resolution (X then Y)

### ECS (Entity Component System)
- ✅ Entity registry with **generations** (safe IDs)
- ✅ Sparse-set component pools (fast + cache friendly)
- ✅ **Built-in components:** `CTransform`, `CVel`, `CCollider`, `CPlayer`, `CSprite`, `CLight`
- ✅ **Systems included:** Player movement, physics, sprite render, lighting gather

### Lighting
- ✅ Tile-based lightmap over the visible region
- ✅ Ambient light control (dark caves / bright day)
- ✅ Light propagation with solid-tile attenuation
- ✅ Drawn as a **darkness overlay** affecting all layers

### Particles
- ✅ Burst emitter
- ✅ Gravity, drag, lifetime fade, size fade
- ✅ Rendered as circles in world space

### UI (immediate mode)
- ✅ Draggable windows
- ✅ Buttons, sliders, checkboxes, labels
- ✅ Clip stack for window contents
- ✅ Stable interaction (hot/active IDs)

---

## Project Layout

```text
/your_project
├── main.cpp
├── wineng.hpp
├── (optional) asset1.png
└── (optional) asset2.png
```

## Build (MinGW g++)
```DOS
g++ main.cpp -O2 -std=c++17 -Wall -Wextra ^
  -lgdi32 -luser32 -lole32 -luuid -lwindowscodecs ^
  -o game.exe
```

## Controls (Default Demo)

    A / D: Move

    Space: Jump

    Mouse wheel: Zoom

    LMB / RMB: Dig / Place tile

## Philosophy

This engine is built for learning internals and shipping small games quickly without dependency pain. The engine code is complex, but the API is kept friendly.
