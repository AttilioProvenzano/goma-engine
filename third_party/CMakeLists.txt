# - Assimp

set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)

# Disable all importers except OBJ, FBX and GLTF
set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF CACHE BOOL "" FORCE)
foreach(name OBJ FBX GLTF)
  set(ASSIMP_BUILD_${name}_IMPORTER ON CACHE BOOL "" FORCE)
endforeach()

# Disable all exporters
set(ASSIMP_BUILD_ALL_EXPORTERS_BY_DEFAULT OFF CACHE BOOL "" FORCE)

add_subdirectory(assimp)
foreach(target assimp IrrXML uninstall UpdateAssimpLibsDebugSymbolsAndDLLs zlib zlibstatic)
  if(TARGET ${target})
	set_property(TARGET ${target} PROPERTY FOLDER ThirdParty/Assimp)
  endif()
endforeach()

# - Catch2

add_library(catch2 INTERFACE)
target_include_directories(catch2 INTERFACE catch2)

# - GLFW

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(glfw)
set_property(TARGET glfw PROPERTY FOLDER ThirdParty)

# - GLM

add_library(glm INTERFACE)
target_include_directories(glm INTERFACE glm)

# - Glslang

set(BUILD_EXTERNAL OFF CACHE BOOL "" FORCE)
set(ENABLE_SPVREMAPPER OFF CACHE BOOL "" FORCE)
set(ENABLE_GLSLANG_BINARIES OFF CACHE BOOL "" FORCE)
set(ENABLE_CTEST OFF CACHE BOOL "" FORCE)
set(ENABLE_HLSL OFF CACHE BOOL "" FORCE)
add_subdirectory(glslang)

foreach(target glslang OGLCompiler OSDependent SPIRV)
  if(TARGET ${target})
    set_property(TARGET ${target} PROPERTY FOLDER ThirdParty/glslang)
  endif()
endforeach()

# - Outcome

add_library(outcome INTERFACE)
target_include_directories(outcome INTERFACE outcome/single-header)

# - SPIRV-Cross

set(SPIRV_CROSS_CLI OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_HLSL OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_MSL OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_CPP OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_REFLECT OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_C_API OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_UTIL OFF CACHE BOOL "" FORCE)
add_subdirectory(SPIRV-Cross)

foreach(target spirv-cross-core spirv-cross-glsl)
  if(TARGET ${target})
    set_property(TARGET ${target} PROPERTY FOLDER ThirdParty/SPIRV-Cross)
  endif()
endforeach()

# - Spdlog

add_library(spdlog INTERFACE)
target_include_directories(spdlog INTERFACE spdlog/include)

# - Stb

add_library(stb INTERFACE)
target_include_directories(stb INTERFACE stb/include)

# - Vulkan

add_library(vulkan
	vma/vk_mem_alloc.cpp
	volk/volk.h
	volk/volk.c)
set_property(TARGET vulkan PROPERTY FOLDER ThirdParty)
target_include_directories(vulkan PUBLIC
	Vulkan-Headers/include
	vma/include
	volk)
