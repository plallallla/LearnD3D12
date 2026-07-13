cmake_minimum_required(VERSION 3.25)

project(LearnD3D12 LANGUAGES CXX)

find_package(glfw3 CONFIG REQUIRED)
find_package(directx-headers CONFIG REQUIRED)

add_subdirectory(shader)

add_executable(playground
        main.cpp
)

add_dependencies(playground compile_shaders)

target_compile_features(playground PRIVATE cxx_std_20)

target_compile_definitions(playground PRIVATE
        GLFW_EXPOSE_NATIVE_WIN32
        SHADER_DIR="${CMAKE_CURRENT_SOURCE_DIR}/shader"
)

target_link_libraries(playground PRIVATE
        glfw
        Microsoft::DirectX-Headers
        d3d12
        dxgi
        dxguid
)

