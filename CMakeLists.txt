cmake_minimum_required(VERSION 3.25)

project(LearnD3D12 LANGUAGES CXX)

find_package(glfw3 CONFIG REQUIRED)
find_package(directx-headers CONFIG REQUIRED)

function(configure_d3d12_target target_name)
    add_dependencies(${target_name} compile_shaders)

    target_compile_features(${target_name} PRIVATE cxx_std_20)

    target_compile_definitions(${target_name} PRIVATE
            GLFW_EXPOSE_NATIVE_WIN32
            SHADER_DIR="${PROJECT_SOURCE_DIR}/shader"
    )

    target_link_libraries(${target_name} PRIVATE
            glfw
            Microsoft::DirectX-Headers
            d3d12
            dxgi
            dxguid
    )
endfunction()

# shader compile script
add_subdirectory(shader)

# projects
add_subdirectory(Triangle)
add_subdirectory(Texture)
