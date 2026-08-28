include(FetchContent)

# Pin SDL3 to an immutable commit so platform builds are reproducible.
set(SDL_SHARED ON CACHE BOOL "Build SDL3 as a shared library" FORCE)
set(SDL_STATIC OFF CACHE BOOL "Build SDL3 as a static library" FORCE)
set(SDL_AUDIO OFF CACHE BOOL "Build SDL3 audio subsystem" FORCE)
set(SDL_GPU OFF CACHE BOOL "Build SDL3 GPU subsystem" FORCE)
set(SDL_RENDER OFF CACHE BOOL "Build SDL3 renderer subsystem" FORCE)
set(SDL_CAMERA OFF CACHE BOOL "Build SDL3 camera subsystem" FORCE)
set(SDL_JOYSTICK OFF CACHE BOOL "Build SDL3 joystick subsystem" FORCE)
set(SDL_HAPTIC OFF CACHE BOOL "Build SDL3 haptic subsystem" FORCE)
set(SDL_SENSOR OFF CACHE BOOL "Build SDL3 sensor subsystem" FORCE)
set(SDL_DIALOG OFF CACHE BOOL "Build SDL3 dialog subsystem" FORCE)
set(SDL_TRAY OFF CACHE BOOL "Build SDL3 tray subsystem" FORCE)
set(SDL_OPENGL OFF CACHE BOOL "Build SDL3 OpenGL support" FORCE)
set(SDL_OPENGLES OFF CACHE BOOL "Build SDL3 OpenGL ES support" FORCE)
set(SDL_VULKAN OFF CACHE BOOL "Build SDL3 Vulkan support" FORCE)
set(SDL_TESTS OFF CACHE BOOL "Build SDL3 tests" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "Build SDL3 examples" FORCE)

FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG 147a8ee32dbf9ac02f3794964490687b6bbda1bc
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(SDL3)
