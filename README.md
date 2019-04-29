# Goma :ocean:

Goma is a simple 3D C++ game engine with Vulkan support.

This is a learning project, meant for experimenting with graphics techniques. It currently lacks most basic features of a full-fledged engine, so it should not be used for anything resembling production! :stuck_out_tongue:

The current plan is to add more features along the way, but I won't be developing/supporting continuously. If you like the project and would like to contribute, issues/PRs are very welcome! :)

![Helmet screenshot](https://user-images.githubusercontent.com/14922868/56866395-e5288080-69d8-11e9-884a-3a3ce0f8e88a.jpg)

# Features

Goma currently supports Windows with a [V-EZ](https://github.com/GPUOpen-LibrariesAndSDKs/V-EZ) backend. I have plans to port it to Android, but it will require a native Vulkan backend first.

These are some of the features that Goma supports:

 * PBR
 * Runtime shader compilation with variants
 * Mipmapping
 * MSAA
 * Mesh culling and sorting
 * Shadow maps (directional lights only)
 * Very basic DoF

# Build

Goma uses the CMake build system. It is tested with Visual Studio 2017 on Windows.

First clone the submodules of this repo:

```
git submodule update --init --recursive
```

Then run CMake:

```
mkdir build && cd build
cmake .. -G "Visual Studio 2017 Win64"
```

Then you can open the Visual Studio solution inside `build` and build it from there.

The target `goma-engine` is meant to be included as a shared library by applications. The test suite `goma-tests` shows usage examples.

# License

Goma is licensed under the MIT license. Feel free to use it however you like! Contributions are accepted under the same license.

This project uses third party dependencies, each of which may have independent licensing:

* [assimp](https://github.com/assimp/assimp): A library to import and export various 3d-model-formats.
* [glfw](https://github.com/glfw/glfw): A multi-platform library for OpenGL, OpenGL ES, Vulkan, window and input.
 * [glm](https://github.com/g-truc/glm): A header only C++ mathematics library for graphics software.
 * [googletest](https://github.com/google/googletest): A testing framework.
 * [outcome](https://github.com/ned14/outcome): Provides lightweight `outcome<T>` and `result<T>`.
 * [spdlog](https://github.com/gabime/spdlog): Fast C++ logging library.
 * [stb](https://github.com/nothings/stb): Single-file public domain (or MIT licensed) libraries for C/C++.
 * [variant](https://github.com/mapbox/variant): An header-only alternative to `boost::variant` for C++11 and C++14.
 * [V-EZ](https://github.com/GPUOpen-LibrariesAndSDKs/V-EZ): A wrapper intended to alleviate the inherent complexity of the Vulkan API.
 * [volk](https://github.com/zeux/volk): Meta loader for Vulkan API.
 * [Vulkan-Headers](https://github.com/KhronosGroup/Vulkan-Headers): Vulkan header files and API registry.

PBR shaders are taken from [glTF-Sample-Viewer](https://github.com/KhronosGroup/glTF-Sample-Viewer) with modifications to adapt them to Vulkan GLSL.

3D models are taken from [glTF-Sample-Models](https://github.com/KhronosGroup/glTF-Sample-Models). Each model may have its own license.

Other credits:

 * Yokohama cubemap texture from [Humus](http://www.humus.name/index.php?page=Textures) (CC-BY 3.0)
 * Cloudy cubemap texture by Spiney from [OpenGameArt](https://opengameart.org/content/cloudy-skyboxes) (CC-BY 3.0)
 * Sphere creation function from [Cute deferred shading](https://github.com/Erkaman/cute-deferred-shading)
