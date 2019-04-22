cmake_minimum_required (VERSION 3.12)

project (GomaEngine C CXX)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -W4")

if(NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")
endif()
if(NOT CMAKE_LIBRARY_OUTPUT_DIRECTORY)
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/lib")
endif()
if(NOT CMAKE_ARCHIVE_OUTPUT_DIRECTORY)
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/lib")
endif()

add_compile_definitions(VK_USE_PLATFORM_WIN32_KHR)
add_compile_definitions(NOMINMAX)

set(PUBLIC_HEADERS
	include/engine.hpp
	include/common/error_codes.hpp
	include/common/include.hpp
	include/common/vez.hpp
	include/input/input.hpp
	include/input/input_system.hpp
	include/renderer/renderer.hpp
	include/renderer/handles.hpp
	include/renderer/backend.hpp
	include/renderer/vez/vez_backend.hpp
	include/renderer/vez/vez_context.hpp
	include/scene/scene.hpp
	include/scene/attachment.hpp
	include/scene/attachments/texture.hpp
	include/scene/attachments/material.hpp
	include/scene/attachments/camera.hpp
	include/scene/attachments/light.hpp
	include/scene/attachments/mesh.hpp
	include/scene/node.hpp
	include/scene/gen_index.hpp
	include/scene/scene_loader.hpp
	include/scene/loaders/assimp_loader.hpp
	include/scripting/scripting_system.hpp
	include/scripting/script.hpp
	include/scripting/scripts/fly_camera.hpp
	include/platform/platform.hpp
	include/platform/win32_platform.hpp
)
set(SOURCES
	src/engine.cpp
	src/input/input_system.cpp
	src/renderer/renderer.cpp
	src/renderer/vez/vez_backend.cpp
	src/scene/scene.cpp
	src/scene/assimp_loader.cpp
	src/platform/win32_platform.cpp
)

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
add_library(goma-engine SHARED
	${PUBLIC_HEADERS}
	${SOURCES}
)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS OFF)

set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/assimp)
foreach(target assimp assimp_cmd IrrXML uninstall UpdateAssimpLibsDebugSymbolsAndDLLs zlib zlibstatic)
  if(TARGET ${target})
	set_property(TARGET ${target} PROPERTY FOLDER ThirdParty/Assimp)
  endif()
endforeach()

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/glfw)
set_property(TARGET glfw PROPERTY FOLDER ThirdParty)

set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
add_subdirectory(third_party/googletest)
foreach(target gtest gtest_main)
  if(TARGET ${target})
    set_property(TARGET ${target} PROPERTY FOLDER ThirdParty/GoogleTest)
  endif()
endforeach()

add_library(volk
	third_party/volk/volk.c)
set_property(TARGET volk PROPERTY FOLDER ThirdParty)

target_include_directories(volk PUBLIC
	third_party/Vulkan-Headers/include
	third_party/volk)

# The top-level CMakeLists.txt for V-EZ sets all output
# directories to an inconvenient path.
# We directly include the subdirectories to avoid this.
set(VEZ_COMPILE_SAMPLES OFF CACHE BOOL "" FORCE)
set(VEZ_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/V-EZ)
find_package(Vulkan REQUIRED)
add_subdirectory(third_party/V-EZ/External/SPIRV-Cross)
set(ENABLE_HLSL OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/V-EZ/External/glslang)
add_subdirectory(third_party/V-EZ/Source)
set_property(TARGET VEZ PROPERTY FOLDER ThirdParty/VEZ)

foreach(target glslang glslang-default-resource-limits OGLCompiler OSDependent SPIRV SPVRemapper
	glslangValidator spirv-remap
)
  if(TARGET ${target})
    set_property(TARGET ${target} PROPERTY FOLDER ThirdParty/VEZ/External/glslang)
  endif()
endforeach()

foreach(target spirv-cross spirv-cross-core spirv-cross-glsl spirv-cross-cpp spirv-cross-hlsl
	spirv-cross-msl spirv-cross-reflect spirv-cross-util
)
  if(TARGET ${target})
    set_property(TARGET ${target} PROPERTY FOLDER ThirdParty/VEZ/External/SPIRV-Cross)
  endif()
endforeach()

target_include_directories(goma-engine PUBLIC
	include
	third_party/glm
	third_party/outcome/single-header
	third_party/spdlog/include
	third_party/stb
	third_party/V-EZ/Source
	third_party/Vulkan-Headers/include)

target_link_libraries(goma-engine PUBLIC assimp glfw VEZ volk)

add_executable(goma-run
	src/main.cpp
)
target_link_libraries(goma-run PUBLIC goma-engine)

add_executable(goma-tests
	tests/tests.cpp
)
target_link_libraries(goma-tests PUBLIC gtest goma-engine)
