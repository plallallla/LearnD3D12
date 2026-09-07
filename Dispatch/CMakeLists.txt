add_executable(Dispatch
        main.cpp
)

configure_d3d12_target(Dispatch)

target_compile_definitions(Dispatch PRIVATE
        DISPATCH_TEXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)
