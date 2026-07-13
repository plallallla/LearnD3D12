cmake_minimum_required(VERSION 3.25)

project(LearnD3D12 LANGUAGES CXX)

find_package(glfw3 CONFIG REQUIRED)
find_package(directx-headers CONFIG REQUIRED)

# Common target and shader helper functions.
include("${CMAKE_CURRENT_SOURCE_DIR}/include/CMakeLists.txt")

# shader compile script
add_subdirectory(shader)

# projects
add_subdirectory(Triangle)
add_subdirectory(Texture)
