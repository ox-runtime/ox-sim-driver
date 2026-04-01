include(FetchContent)

# Bring in the vog windowing library (fetches GLFW + ImGui internally)
FetchContent_Declare(vog
    GIT_REPOSITORY https://github.com/ox-runtime/vog.git
    GIT_TAG v1.1.11
)
FetchContent_MakeAvailable(vog)

if(APPLE)
    enable_language(OBJCXX)
endif()

if(NOT TARGET clip)
    set(CLIP_ENABLE_IMAGE ON CACHE BOOL "" FORCE)
    set(CLIP_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(CLIP_TESTS OFF CACHE BOOL "" FORCE)
    set(CLIP_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(clip
        GIT_REPOSITORY https://github.com/dacap/clip.git
        GIT_TAG main
        GIT_SHALLOW true
    )
    FetchContent_MakeAvailable(clip)
endif()

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
    clip
    PARENT_SCOPE
)
