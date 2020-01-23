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

add_executable(goma-tests
	tests/tests.cpp
)
target_link_libraries(goma-tests PUBLIC engine gtest)
