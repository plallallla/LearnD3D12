include_guard(GLOBAL)

function(configure_d3d12_target target_name)
    add_dependencies(${target_name} compile_shaders)

    target_compile_features(${target_name} PRIVATE cxx_std_20)

    target_include_directories(${target_name} PRIVATE
            "${CMAKE_CURRENT_FUNCTION_LIST_DIR}"
    )

    target_compile_definitions(${target_name} PRIVATE            
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

function(compile_shader shader_name)
    set(options DEBUG WARNINGS_AS_ERRORS)
    set(one_value_args VS_ENTRY PS_ENTRY VS_PROFILE PS_PROFILE)
    cmake_parse_arguments(SHADER "${options}" "${one_value_args}" "" ${ARGN})

    if (NOT SHADER_VS_ENTRY)
        set(SHADER_VS_ENTRY VSMain)
    endif ()
    if (NOT SHADER_PS_ENTRY)
        set(SHADER_PS_ENTRY PSMain)
    endif ()
    if (NOT SHADER_VS_PROFILE)
        set(SHADER_VS_PROFILE vs_6_0)
    endif ()
    if (NOT SHADER_PS_PROFILE)
        set(SHADER_PS_PROFILE ps_6_0)
    endif ()

    set(shader_flags)
    if (SHADER_DEBUG)
        list(APPEND shader_flags -Zi -Qembed_debug -Od)
    else ()
        list(APPEND shader_flags -O3)
    endif ()

    if (SHADER_WARNINGS_AS_ERRORS)
        list(APPEND shader_flags -WX)
    endif ()

    set(shader_source "${CMAKE_CURRENT_SOURCE_DIR}/${shader_name}.hlsl")
    set(vertex_shader_output "${CMAKE_CURRENT_SOURCE_DIR}/${shader_name}_${SHADER_VS_ENTRY}.cso")
    set(pixel_shader_output "${CMAKE_CURRENT_SOURCE_DIR}/${shader_name}_${SHADER_PS_ENTRY}.cso")

    add_custom_command(
            OUTPUT "${vertex_shader_output}"
            COMMAND "${DXC_EXECUTABLE}" -T "${SHADER_VS_PROFILE}" -E "${SHADER_VS_ENTRY}" ${shader_flags} -Fo "${vertex_shader_output}" "${shader_source}"
            DEPENDS "${shader_source}"
            COMMENT "Compiling ${shader_name} vertex shader"
            VERBATIM
    )

    add_custom_command(
            OUTPUT "${pixel_shader_output}"
            COMMAND "${DXC_EXECUTABLE}" -T "${SHADER_PS_PROFILE}" -E "${SHADER_PS_ENTRY}" ${shader_flags} -Fo "${pixel_shader_output}" "${shader_source}"
            DEPENDS "${shader_source}"
            COMMENT "Compiling ${shader_name} pixel shader"
            VERBATIM
    )

    set(COMPILED_SHADER_OUTPUTS
            ${COMPILED_SHADER_OUTPUTS}
            "${vertex_shader_output}"
            "${pixel_shader_output}"
            PARENT_SCOPE
    )
endfunction()
