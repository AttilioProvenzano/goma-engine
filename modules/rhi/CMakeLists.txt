include(${CMAKE_SOURCE_DIR}/scripts/cmake/add_module.cmake)
add_module(common platform vulkan glslang SPIRV spirv-cross-glsl)
