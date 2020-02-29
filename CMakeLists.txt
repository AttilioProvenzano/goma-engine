cmake_minimum_required (VERSION 3.12)

project (GomaEngine C CXX)
set(CMAKE_CXX_STANDARD 14)

if(NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")
endif()
if(NOT CMAKE_LIBRARY_OUTPUT_DIRECTORY)
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/lib")
endif()
if(NOT CMAKE_ARCHIVE_OUTPUT_DIRECTORY)
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/lib")
endif()

add_compile_definitions(NOMINMAX)

add_compile_definitions(VK_USE_PLATFORM_WIN32_KHR)

add_subdirectory(modules)
add_subdirectory(third_party)

if(MSVC)
	target_compile_options(engine PRIVATE /W3)
else()
	target_compile_options(engine -Wall -Wextra -pedantic)
endif()

target_compile_definitions(engine PUBLIC
	GOMA_ASSETS_DIR="${PROJECT_SOURCE_DIR}/assets/")

add_executable(goma-tests
  tests/goma_tests.hpp
	tests/rendering_abstraction_tests.cpp
	tests/scene_tests.cpp
	tests/test_main.cpp
)
target_link_libraries(goma-tests PUBLIC catch2 engine)
