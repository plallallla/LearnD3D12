include_guard(GLOBAL)

function(configure_d3d12_target target_name)
    add_dependencies(${target_name} compile_shaders)

    target_compile_features(${target_name} PRIVATE cxx_std_20)

    target_compile_definitions(${target_name} PRIVATE
            SHADER_DIR="${PROJECT_SOURCE_DIR}/shader"
    )

    target_link_libraries(${target_name} PRIVATE
            LearnD3D12::Headers
            Microsoft::DirectX-Headers
            d3d12
            dxgi
            dxguid
    )
endfunction()
