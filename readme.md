# ClayMarble

**A deterministic, ECS-driven game engine in pure C**

ClayMarble is a ground-up rewrite of MarbleEngine's core systems in C, designed for reproducible simulation, cross-platform deployment (native Windows/Linux + WebAssembly), and predictable performance characteristics. The engine prioritizes determinism, architectural clarity, and measurable behavior over high-level abstractions.

## Design Philosophy

- **Determinism First**: Fixed-timestep tick loop with deterministic PRNG (SplitMix32) ensures identical simulation results across platforms given the same input sequence
- **ECS Architecture**: Sparse set-based component storage with explicit system scheduling
- **Explicit Resource Management**: Manual memory control, no hidden allocations, predictable performance
- **Measured Optimization**: Profile-driven rather than assumption-driven performance work

## Core Features

### Simulation Layer
- **Fixed Tick Rate**: 64Hz simulation loop with configurable catchup limits
- **Entity-Component-System**: Sparse set component pools with O(1) lookups
- **Deterministic PRNG**: Per-interaction seeding for full replay capability
- **Command Buffer Pattern**: Deferred state mutations for temporal consistency

### Rendering Pipeline
- **OpenGL ES 2.0**: Maximum compatibility across desktop and web targets
- **Framebuffer-Based Upscaling**: Low-resolution render targets with pixel-perfect scaling
- **Retro Aesthetics**: Configurable color quantization, affine texture mapping emulation
- **Shader-Driven Effects**: Vertex color mixing, depth testing control, custom post-processing

### Data Layer
- **MarbleScript DSL**: Declarative entity/component definitions
- **Binary Caching**: Compiled world states for instant load times
- **Lua Integration**: Gameplay logic scripting with C backend for hot systems

## Architecture Overview

```
┌─────────────────────────────────────────┐
│         Platform Layer (SDL2)            │
│  Window Management, Input, Timing        │
└─────────────────┬───────────────────────┘
                  │
┌─────────────────▼───────────────────────┐
│       Bridge Engine (OpenGL ES 2)        │
│  Render State, Texture Mgmt, Shaders     │
└─────────────────┬───────────────────────┘
                  │
┌─────────────────▼───────────────────────┐
│         Core Simulation Loop             │
│  Fixed Tick @ 64Hz, System Dispatcher    │
└─────────────────┬───────────────────────┘
                  │
        ┌─────────┴─────────┐
        │                   │
┌───────▼────────┐  ┌──────▼──────────┐
│  ECS Storage   │  │  Game Systems   │
│  Sparse Sets   │  │  Interaction    │
│  Components    │  │  Physics/AI     │
└────────────────┘  └─────────────────┘
```

## Current Status: Phase 0.2

**Validation Phase**: Core engine systems operational with deterministic replay verified across platforms.

### Implemented
- ✅ Entity ID allocator with monotonic bump allocation
- ✅ Sparse set component pools (8 component types)
- ✅ Fixed-timestep tick loop with overflow protection
- ✅ Deterministic interaction system with per-action PRNG seeding
- ✅ Material layer system with hardness-based damage resolution
- ✅ Body part targeting and fine motor skill requirements
- ✅ OpenGL ES 2.0 renderer with FBO-based upscaling
- ✅ Cross-platform timing (microsecond precision)

### In Progress
- 🔄 Spatial partitioning for entity queries
- 🔄 Command buffer for deferred mutations
- 🔄 Save/load serialization format
- 🔄 Lua VM integration for gameplay scripts

### Roadmap
- Asset pipeline: Blender → sprite atlas generation
- Network synchronization layer (lockstep with hash verification)
- Memory profiling tooling
- Advanced rendering: shadow mapping, dynamic lighting

## Technical Details

### Deterministic Simulation

All randomness is sourced from a seeded PRNG with explicit state management:

```c
McRng rng;
mc_rng_seed(&rng, WORLD_SEED 
            ^ (uint32_t)tick
            ^ (actor_id * 2654435761u) 
            ^ (target_id * 2246822519u));
```

Same seed → same tick → same entities → identical results. Critical for:
- Replay systems
- Network multiplayer (client prediction)
- Automated testing
- Debugging temporal issues

### Component Storage

Sparse sets provide O(1) component access while maintaining dense iteration:

```c
typedef struct {
    uint32_t  count;
    EntityID* entities;    // Dense array
    uint32_t* sparse;      // Entity → index mapping
    void*     components;  // Dense component data
    size_t    comp_size;
} SparseSet;
```

Trade-off: Memory overhead for predictable performance. Dense iteration is cache-friendly; no pointer chasing.

### System Scheduling

Systems run at configurable frequencies to balance simulation fidelity and performance:

```c
static const uint32_t SYSTEM_FREQ[SYS_COUNT] = {
    1,  // Tick log: every frame
    2,  // Interaction: every 2 ticks (32Hz)
    3,  // World status: every 3 ticks (21Hz)
};
```

Hot systems (physics, AI) run at full rate; cold systems (save games, analytics) run slower.

## Build Instructions

### Prerequisites
- GCC 9+ or Clang 10+ (C11 support required)
- SDL2 development libraries
- OpenGL ES 2.0 headers
- Make or CMake

### Native Build
```bash
# Linux
gcc -std=c11 -Wall -Wextra -O2 main.c -lSDL2 -lGLESv2 -lm -o claymarble

# Windows (MinGW)
gcc -std=c11 -Wall -Wextra -O2 main.c -lmingw32 -lSDL2main -lSDL2 -lopengl32 -o claymarble.exe
```

### WebAssembly Build
```bash
emcc -std=c11 -O2 main.c bridge_engine.c \
  -s USE_SDL=2 -s USE_WEBGL2=1 -s FULL_ES2=1 \
  -s ALLOW_MEMORY_GROWTH=1 \
  -o claymarble.html
```

## Project Context

ClayMarble exists as a testbed for deterministic simulation architecture and cross-platform deployment strategies. The engine's design emphasizes predictability over feature breadth—ideal for projects requiring exact replay, network synchronization, or automated testing.

### MINDMARR: Implementation Showcase

Included as a demonstration project is **MINDMARR**, a d100 roll-under survival horror roguelike set on sentient Mars. This father-son collaborative project (implementing my son's game design ideas) showcases:

- Real-time FOV raycast with tile visibility tracking
- Turn-based combat with percentile roll resolution
- Multi-layered entity system (body parts, materials, armor)
- Procedural dungeon generation with room connectivity
- Particle effects and screen shake feedback
- Modal UI state management (title/playing/levelup/death)

MINDMARR serves as a practical stress test for the engine's systems while remaining appropriately scoped for a collaborative hobby project. All game logic lives in Lua scripts, demonstrating the scripting layer's capabilities.

## Contributing

This is a personal research project, but feedback on architecture decisions and performance characteristics is welcome. File issues for:
- Determinism violations (same input producing different results)
- Cross-platform build failures
- Performance regressions with profiling data

## License

TBD.

## Acknowledgments

Built on 8+ years of engine iteration, with architectural lessons learned from MarbleEngine, Project Bridge (ECS prototyping), and production game development experience.

---

**Status**: Active development  
**Platform**: Windows, Linux, WebAssembly  
**Language**: C99
**Dependencies**: SDL2, OpenGL ES 2.0, Lua 5.4 (optional)