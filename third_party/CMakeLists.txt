# - Assimp

set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)

# Disable all importers except OBJ, FBX and GLTF
set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF CACHE BOOL "" FORCE)
foreach(name OBJ FBX GLTF)
  set(ASSIMP_BUILD_${name}_IMPORTER ON CACHE BOOL "" FORCE)
endforeach()

add_subdirectory(assimp)
foreach(target assimp assimp_cmd IrrXML uninstall UpdateAssimpLibsDebugSymbolsAndDLLs zlib zlibstatic)
  if(TARGET ${target})
	set_property(TARGET ${target} PROPERTY FOLDER ThirdParty/Assimp)
  endif()
endforeach()

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

# - GoogleTest

set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
add_subdirectory(googletest)
foreach(target gtest gtest_main)
  if(TARGET ${target})
    set_property(TARGET ${target} PROPERTY FOLDER ThirdParty/GoogleTest)
  endif()
endforeach()

# - Outcome

add_library(outcome INTERFACE)
target_include_directories(outcome INTERFACE outcome/single-header)

# - Spdlog

add_library(spdlog INTERFACE)
target_include_directories(spdlog INTERFACE spdlog/include)

# - Volk

add_library(vulkan
	volk/volk.h
	volk/volk.c)
set_property(TARGET vulkan PROPERTY FOLDER ThirdParty)
target_include_directories(vulkan PUBLIC
	Vulkan-Headers/include
	volk)
