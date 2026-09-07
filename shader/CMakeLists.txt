set(COMPILED_SHADER_OUTPUTS)

compile_shader(triangle DEBUG WARNINGS_AS_ERRORS)
compile_shader(pbr_sphere DEBUG WARNINGS_AS_ERRORS)
compile_shader(dispatch DEBUG WARNINGS_AS_ERRORS)
compile_compute_shader(dispatch CS_ENTRY CSMain CS_PROFILE cs_6_0 DEBUG WARNINGS_AS_ERRORS)

add_custom_target(compile_shaders
        DEPENDS
        ${COMPILED_SHADER_OUTPUTS}
)
