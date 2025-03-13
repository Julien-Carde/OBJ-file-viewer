# OBJ Viewer

A lightweight, cross-platform 3D model viewer for OBJ files with smooth shading and wireframe toggle. This is a project I completed this year to experiment with openGL.

## Features

- Fast and efficient OBJ file loading
- High-quality rendering with smooth shading
- Interactive rotation and zoom controls
- Wireframe toggle with a single key press
- Support for various OBJ file formats (triangles, quads, with or without normals)
- Automatic normal calculation for smooth surfaces
- 8x anti-aliasing for crisp rendering

## Requirements

- OpenGL 3.3+ compatible graphics card
- GLEW (OpenGL Extension Wrangler Library)
- GLFW (OpenGL Framework)
- GLM (OpenGL Mathematics)

## Installation

### macOS

Using Homebrew:

```bash
# Install dependencies
brew install glew glfw glm
```

### Linux

```bash
# Ubuntu/Debian
sudo apt-get install libglew-dev libglfw3-dev libglm-dev

# Fedora
sudo dnf install glew-devel glfw-devel glm-devel
```

### Windows

It's recommended to use vcpkg:

```bash
vcpkg install glew:x64-windows glfw3:x64-windows glm:x64-windows
```

## Compilation

### macOS

```bash
g++ -o OBJ_Viewer 3D.cpp -I/usr/local/include -L/usr/local/lib -lglew -lglfw -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo
```

### Linux

```bash
g++ -o OBJ_Viewer 3D.cpp -lGLEW -lglfw -lGL -lm
```

### Windows (with MSVC)

```bash
cl 3D.cpp /EHsc /I"path\to\include" /link /LIBPATH:"path\to\lib" glew32.lib glfw3.lib opengl32.lib
```

## Usage

```bash
./OBJ_Viewer path/to/your/model.obj
```

## Controls

- **Mouse drag**: Rotate the model
- **Scroll wheel**: Zoom in/out
- **W key**: Toggle wireframe mode
- **Esc key**: Exit application
