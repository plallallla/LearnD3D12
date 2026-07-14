set(COMPILED_SHADER_OUTPUTS)

compile_shader(triangle DEBUG WARNINGS_AS_ERRORS)

add_custom_target(compile_shaders
        DEPENDS
        ${COMPILED_SHADER_OUTPUTS}
)
