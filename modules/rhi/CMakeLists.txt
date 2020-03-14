include(${CMAKE_SOURCE_DIR}/scripts/cmake/add_module.cmake)
add_module(common platform vulkan glslang glslang-default-resource-limits SPIRV spirv-cross-glsl)
