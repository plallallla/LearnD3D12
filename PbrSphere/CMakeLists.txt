add_executable(PbrSphere
        main.cpp
)

configure_d3d12_target(PbrSphere)

target_compile_definitions(PbrSphere PRIVATE
        PBR_TEXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/textures"
)
