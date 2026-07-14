include_guard(GLOBAL)

find_package(glfw3 CONFIG REQUIRED)
find_package(directx-headers CONFIG REQUIRED)

find_program(DXC_EXECUTABLE NAMES dxc dxc.exe)
if (NOT DXC_EXECUTABLE AND WIN32)
    file(GLOB DXC_EXECUTABLE_CANDIDATES "C:/Program Files (x86)/Windows Kits/10/bin/*/x64/dxc.exe")
    list(SORT DXC_EXECUTABLE_CANDIDATES ORDER DESCENDING)
    list(LENGTH DXC_EXECUTABLE_CANDIDATES DXC_EXECUTABLE_CANDIDATE_COUNT)
    if (DXC_EXECUTABLE_CANDIDATE_COUNT GREATER 0)
        list(GET DXC_EXECUTABLE_CANDIDATES 0 DXC_EXECUTABLE)
    endif ()
endif ()

if (NOT DXC_EXECUTABLE)
    message(FATAL_ERROR "dxc executable not found. Install the Windows SDK or add dxc.exe to PATH.")
endif ()
