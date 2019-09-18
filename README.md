# Progression [![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](https://opensource.org/licenses/MIT)

## Build status
- Linux build: [![Build Status](https://travis-ci.org/LiamTyler/Progression.svg?branch=FPS-in-city)](https://travis-ci.org/LiamTyler/Progression)
- Windows build: [![Build status](https://ci.appveyor.com/api/projects/status/3badv9456nqrow5f?svg=true)](https://ci.appveyor.com/project/LiamTyler/progression)

## Description
A C++ game engine I have been developing for Linux and Windows. 

## Features
- OpenGL rendering system
- Forward and Tiled deferred rendering pipelines
- Entity component architecture
- Resource manager with custom model format for optimized loading
- Scene file loading 
- Basic positional audio
- Window management
- Keyboard and mouse handling

## Installing + Configuring
```
git clone --recursive https://github.com/LiamTyler/Progression.git
cd deferred-starter
mkdir build
cd build
cmake ..
```

CMake Options:
- PROGRESSION_BUILD_EXAMPLES (defaults to on)
- PROGRESSION_BUILD_TOOLS (defaults to on)
- PROGRESSION_AUDIO (defaults to off, requires OpenAL and sndfile)

## Compiling
For Linux: just do a `make -j`

For Windows: I always do `cmake -G "Visual Studio 15 2017 Win64" ..` instead of `cmake ..` to get the 64 bit version. Then open up the solution file, switch to Release mode, and build like normal in VS
