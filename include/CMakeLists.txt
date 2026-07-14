add_library(LearnD3D12Headers INTERFACE)
add_library(LearnD3D12::Headers ALIAS LearnD3D12Headers)

target_include_directories(LearnD3D12Headers INTERFACE
        "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_sources(LearnD3D12Headers INTERFACE
        "${CMAKE_CURRENT_SOURCE_DIR}/GlfwWindow.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/Utility.hpp"
)

target_link_libraries(LearnD3D12Headers INTERFACE
        glfw
)
