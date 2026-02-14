# Physics Integration Test - Dynamic Parkour

A 3D physics-based character controller and environment prototype built using **Jolt Physics**, **Raylib**, and a custom **Header-Only ECS**. This project demonstrates advanced character movement, an intelligent third-person camera system, and dynamic world-building.

## Features

### 🕹️ Character Controller
- **Advanced Locomotion:** Smooth acceleration/deceleration curves and ground adherence.
- **Double Jump & Coyote Time:** Responsive vertical movement with a forgiving 0.2s coyote time window.
- **Slope Handling:** Intelligent traversal of steep inclines and stairs using Jolt's `ExtendedUpdate`.
- **Wall Sliding:** Prevents "sticking" to vertical surfaces; gravity correctly pulls the character down walls.

### 🎥 Intelligent Camera System
- **Dynamic Follow:** Smartly orbits to stay behind the player when moving away, but suppresses auto-rotation when walking toward the camera to prevent spiraling.
- **Lazy Centering:** Smooth interpolation that scales with movement speed for a professional feel.
- **Manual Override:** 1.0s delay after mouse/stick input before auto-follow re-engages.
- **Zoom States:** Three toggleable zoom levels (Tight, Medium, Wide).

### 🛠️ Real-time Building
- **Platform Planting:** Create your own staircase or floating path by spawning platforms directly beneath the character during jumps.
- **Deferred ECS Commands:** Uses a `CommandBuffer` to safely spawn entities during system iteration, avoiding structural change errors.

## Controls

| Action | Keyboard | Gamepad |
| :--- | :--- | :--- |
| **Move** | `W`, `A`, `S`, `D` / `LMB` (Fwd) | Left Stick |
| **Jump** | `Space` | South Button (A/Cross) |
| **Orbit Camera** | `Right Mouse` | Right Stick |
| **Zoom** | `Scroll` / `Z`, `X` | Left/Right Bumpers |
| **Toggle Follow** | `C` | West Button (X/Square) |
| **Plant Platform** | `E` | Right Trigger |
| **Reset Scene** | `R` | - |

## Project Structure

```text
├── extern/
│   └── ecs/             # Custom Header-only ECS (Git Submodule)
├── resources/
│   └── shaders/         # GLSL Lighting and Shadow shaders
├── src/
│   ├── systems/         # Modular Logic Systems
│   │   ├── builder.hpp  # Platform spawning logic
│   │   ├── camera.hpp   # Orbit and Follow logic
│   │   ├── character.hpp# Physics character controller
│   │   ├── gamepad.hpp  # Controller polling
│   │   ├── physics.hpp  # Jolt simulation wrapper
│   │   └── renderer.hpp # 3D Rendering & UI
│   ├── components.hpp   # Data definitions
│   ├── main.cpp         # Entry point & System registration
│   └── physics_context.hpp # Jolt initialization
└── CMakeLists.txt       # Cross-platform build configuration
```

## Build Instructions

### Prerequisites
- **CMake** (v3.18+)
- **C++20 Compiler** (GCC, Clang, or MSVC)
- **Git**

### Compilation (Linux)
```bash
# Clone with submodules
git clone --recursive <repo-url>
cd physics-integration-test

# Configure and build
cmake -B build -DCMAKE_BUILD_TIME=Release
cmake --build build

# Run
./build/demo
```

### Compilation (Windows)
1. Ensure you have **Visual Studio 2022** or **MinGW** installed.
2. Open the folder in Visual Studio or use the command line:
```powershell
cmake -B build
cmake --build build --config Release
.\build\Release\demo.exe
```
*Note: The build process automatically copies the `resources` folder to the output directory via a post-build command.*

## Architecture Notes
- **ECS-Driven:** High decoupling between input, physics, and rendering.
- **Stateless Systems:** Logic systems use static `Update` methods that iterate over component queries.
- **Physics Integration:** Jolt simulation runs at a fixed step, synchronized with the ECS `WorldTransform` components.
