
#  Vulkan Racer

A real-time arcade racing game built using **C++20, SDL3, and Vulkan 1.4**.

Vulkan Racer uses a custom Vulkan graphics pipeline and GLSL shaders to render a pseudo-3D racing environment with dynamic road movement, rival cars, steering, acceleration, and a real-time HUD.

---

##  Features

- Vulkan-powered graphics rendering
- SDL3 window and input management
- C++20 codebase
- Dynamic pseudo-3D racing environment
- Animated racing track
- Player-controlled sports car
- AI-style rival traffic
- Acceleration and braking
- Steering and road movement
- Real-time speed indicator
- Score tracking
- Resizable application window
- Vulkan swapchain and presentation
- Dynamic rendering
- High-resolution rendering support

---

## 🛠️ Technologies Used

| Technology | Purpose |
|---|---|
| C++20 | Core programming language |
| Vulkan 1.4 | GPU graphics API |
| SDL3 | Window and input management |
| GLSL 450 | GPU shader programming |
| CMake | Build configuration |
| Ninja / MSVC | Build tools |
| Vulkan SDK | Vulkan headers, libraries and shader compiler |
| CLion | Integrated development environment |

---

##  Dependencies

The following software is required to build and run the project.

### 1. C++ Compiler

- Visual Studio 2022
- MSVC C++ compiler
- C++20 support

Install the **Desktop development with C++** workload.

### 2. Vulkan SDK

Download:

https://vulkan.lunarg.com/sdk/home

Recommended SDK version:

`Vulkan SDK 1.4.357.0`

The SDK provides:

- Vulkan headers
- Vulkan loader library
- GLSL shader compiler (`glslc`)
- Vulkan development tools

Expected Windows installation:

```text
C:/VulkanSDK/1.4.357.0/
```

### 3. SDL3

SDL3 is downloaded automatically through CMake FetchContent.

Official repository:

https://github.com/libsdl-org/SDL

Version used:

`release-3.2.0`

Manual SDL installation is not required.

### 4. CMake

Minimum required version:

`3.24`

Download:

https://cmake.org/download/

### 5. Git

Git is required by CMake FetchContent to download SDL3.

Download:

https://git-scm.com/downloads

### 6. CLion

Recommended IDE:

https://www.jetbrains.com/clion/

---

##  Project Structure

```text
VulkanRacer/
│
├── CMakeLists.txt
│
├── src/
│   └── main.cpp
│
├── shaders/
│   ├── racer.vert
│   └── racer.frag
│
└── README.md
```

### File Description

| File | Description |
|---|---|
| `CMakeLists.txt` | Project configuration and dependencies |
| `main.cpp` | Vulkan initialization, game loop and rendering |
| `racer.vert` | Vertex shader |
| `racer.frag` | Fragment shader and racing scene |

---

##  Installation

### Step 1: Clone the Repository through Git CLI

```bash
gh repo clone arcZen75/carFlare-v2
```

### Step 2: Open in CLion

Open the extracted project directory in CLion.

### Step 3: Configure Vulkan SDK

Make sure the Vulkan SDK is installed.

The project expects:

```text
C:/VulkanSDK/1.4.357.0/Include
C:/VulkanSDK/1.4.357.0/Lib/vulkan-1.lib
C:/VulkanSDK/1.4.357.0/Bin/glslc.exe
```

If your SDK is installed elsewhere, update `VULKAN_SDK_ROOT` in `CMakeLists.txt`.

### Step 4: Configure CMake

Reload the CMake project.

CMake will automatically:

1. Find Vulkan.
2. Download SDL3.
3. Configure SDL3.
4. Compile GLSL shaders.
5. Generate SPIR-V shader binaries.
6. Link the executable.

### Step 5: Build

```bash
cmake -S . -B build -G Ninja
cmake --build build
```

### Step 6: Run

Run the `VulkanRacer` executable from CLion or your build directory.

---

##  Game Controls

| Key | Action |
|---|---|
| W | Accelerate |
| ↑ | Accelerate |
| S | Brake |
| ↓ | Brake |
| A | Steer Left |
| ← | Steer Left |
| D | Steer Right |
| → | Steer Right |
| ESC | Exit Game |

---

##  Graphics Architecture

The application uses the Vulkan graphics pipeline.

Rendering flow:

```text
C++ Application
       |
       v
     SDL3
       |
       v
 Vulkan Instance
       |
       v
 Vulkan Surface
       |
       v
 Physical Device
       |
       v
 Logical Device
       |
       v
   Swapchain
       |
       v
 Graphics Pipeline
       |
       v
 Vertex Shader
       |
       v
 Fragment Shader
       |
       v
   Framebuffer
       |
       v
 Screen Output
```

### Rendering Features

- Vulkan dynamic rendering
- GPU-based fragment shading
- SPIR-V shader compilation
- Swapchain image presentation
- Synchronization with semaphores and fences
- Window resizing support

---

##  System Requirements

| Component | Requirement |
|---|---|
| OS | Windows 10 / 11 |
| Architecture | x64 |
| CPU | Modern x64 processor |
| GPU | Vulkan-compatible graphics card |
| RAM | 4 GB minimum recommended |
| Vulkan | Vulkan 1.3+ GPU support |
| Compiler | MSVC with C++20 |
| Display | 1280 × 720 or higher recommended |

For 4K output, use a compatible 4K display and configure the window accordingly.

---

##  Current Development Status

- [x] SDL3 initialization
- [x] Vulkan instance creation
- [x] Vulkan surface creation
- [x] Physical device selection
- [x] Logical device creation
- [x] Swapchain creation
- [x] Graphics pipeline
- [x] GLSL shaders
- [x] Real-time rendering
- [x] Player steering
- [x] Acceleration and braking
- [x] Animated racing environment
- [x] Rival traffic visuals
- [x] Score tracking

### Planned Improvements

- [ ] True 3D car models
- [ ] Real 3D racing track geometry
- [ ] Depth buffering
- [ ] Texture mapping
- [ ] PBR materials
- [ ] Dynamic lighting and shadows
- [ ] Real AI opponent behavior
- [ ] Collision physics
- [ ] Lap system
- [ ] Sound effects
- [ ] Main menu
- [ ] Graphics settings
- [ ] Multiplayer support

---

##  Learning Resources

- Vulkan Tutorial: https://vulkan-tutorial.com/
- Vulkan Specification: https://docs.vulkan.org/spec/latest/
- SDL3 Documentation: https://wiki.libsdl.org/SDL3/
- SDL3 Repository: https://github.com/libsdl-org/SDL
- GLSL Documentation: https://docs.vulkan.org/glsl/latest/
- CMake Documentation: https://cmake.org/documentation/

---

## License

This project is intended for educational and personal development purposes.

SDL3 is licensed under the zlib license. Vulkan is an API specification maintained by Khronos Group.

---


---

⭐ If you like this project, consider giving the repository a star!
