include(FetchContent)

# Bring in the vog windowing library (fetches GLFW + ImGui internally)
FetchContent_Declare(vog
    GIT_REPOSITORY https://github.com/ox-runtime/vog.git
    GIT_TAG v1.1.8
)
FetchContent_MakeAvailable(vog)

set(GUI_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/gui_window.cpp
    PARENT_SCOPE
)

set(GUI_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}
    PARENT_SCOPE
)

set(GUI_LIBRARIES
    vog
    PARENT_SCOPE
)
