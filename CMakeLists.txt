cmake_minimum_required(VERSION 3.18)
if(${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.5")
  cmake_policy(SET CMP0048 NEW)
endif()
project(physics_integration_test CXX)

set(CMAKE_CXX_STANDARD 20) # Jolt prefers C++20/17
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- Dependencies ---

# 1. ECS (Git Submodule in extern/ecs)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/extern/ecs/CMakeLists.txt")
  add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/extern/ecs"
                   "${CMAKE_BINARY_DIR}/ecs_build" EXCLUDE_FROM_ALL)
else()
  message(FATAL_ERROR "ECS submodule not found in extern/ecs. Did you run 'git submodule update --init'?")
endif()

# 1. Jolt Physics (FetchContent)
include(FetchContent)
message(STATUS "Fetching Jolt Physics...")
FetchContent_Declare(
  JoltPhysics
  GIT_REPOSITORY https://github.com/jrouwe/JoltPhysics.git
  GIT_TAG v5.2.0
  SOURCE_SUBDIR Build)

# Jolt Build Options
if(MSVC)
  # Jolt defaults to static runtime (MT), but we (and Raylib) use dynamic (MD)
  set(USE_STATIC_MSVC_RUNTIME_LIBRARY OFF CACHE BOOL "" FORCE)
endif()

set(USE_SSE4_2
    ON
    CACHE BOOL "" FORCE)
set(TARGET_UNIT_TESTS
    OFF
    CACHE BOOL "" FORCE)
set(TARGET_HELLO_WORLD
    OFF
    CACHE BOOL "" FORCE)
set(TARGET_PERFORMANCE_TEST
    OFF
    CACHE BOOL "" FORCE)
set(TARGET_VIEWER
    OFF
    CACHE BOOL "" FORCE)
set(CROSS_PLATFORM_DETERMINISTIC
    OFF
    CACHE BOOL "" FORCE) # Simpler for now

FetchContent_MakeAvailable(JoltPhysics)

# 1. Raylib (FetchContent)
message(STATUS "Fetching Raylib...")
FetchContent_Declare(
  raylib
  GIT_REPOSITORY https://github.com/raysan5/raylib.git
  GIT_TAG master)
set(BUILD_EXAMPLES
    OFF
    CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(raylib)

# --- Main Executable ---

add_executable(
  demo 
  src/main.cpp 
  src/systems/builder.cpp
  src/systems/camera.cpp
  src/systems/character.cpp
  src/systems/gamepad.cpp
  src/systems/keyboard.cpp
  src/systems/physics.cpp
  src/systems/renderer.cpp
)

# Debug Jolt Path
message(STATUS "Jolt Source: ${JoltPhysics_SOURCE_DIR}")
message(STATUS "joltphysics Source: ${joltphysics_SOURCE_DIR}")

# Include Paths
target_include_directories(demo PRIVATE src ${joltphysics_SOURCE_DIR}
                                        ${JoltPhysics_SOURCE_DIR})

# Linking
target_link_libraries(demo PRIVATE ecs Jolt raylib)

# Windows-specific system libraries for Raylib
if(WIN32)
  target_link_libraries(demo PRIVATE winmm gdi32 opengl32 shell32 user32)
endif()

# Copy resources to build directory so they are found relative to executable
add_custom_command(TARGET demo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/resources
    $<TARGET_FILE_DIR:demo>/resources
)

# Enable aggressive optimization for Release builds
if(CMAKE_BUILD_TYPE MATCHES Release)
  if(MSVC)
    target_compile_options(demo PRIVATE /O2)
  else()
    target_compile_options(demo PRIVATE -O3)
  endif()
endif()
