cmake_minimum_required(VERSION 3.25)

project(LearnD3D12 LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(Dependencies)
include(D3D12Target)
include(ShaderCompiler)

# shared headers
add_subdirectory(include)

# shader compile script
add_subdirectory(shader)

# projects
add_subdirectory(Triangle)
add_subdirectory(Texture)
