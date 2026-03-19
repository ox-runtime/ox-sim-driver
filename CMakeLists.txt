cmake_minimum_required(VERSION 3.27)

project(ox-simulator VERSION 0.4.0 LANGUAGES CXX)

if(WIN32 AND MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

if(NOT WIN32)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
option(OX_SIM_BUILD_EXAMPLES "Build simulator API examples" ON)
option(OX_SIM_BUILD_TESTS "Build simulator API tests" ON)

include(CTest)

if(NOT TARGET spdlog::spdlog)
    include(FetchContent)

    FetchContent_Declare(
        spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog
        GIT_TAG v1.17.0
        GIT_SHALLOW true
    )
    if(NOT WIN32)
        set(SPDLOG_BUILD_PIC ON CACHE BOOL "" FORCE)
    endif()
    FetchContent_MakeAvailable(spdlog)
endif()

set(OUTPUT_FOLDER "ox_simulator")

# Prefer GLVND on Linux so FindOpenGL resolves the split libOpenGL/libGLX packages
# used on modern distributions and GitHub runners.
if(UNIX AND NOT APPLE)
    set(OpenGL_GL_PREFERENCE GLVND)
endif()

# Add subdirectories
add_subdirectory(src/gui)

# Source files
set(SIMULATOR_CORE_SOURCES
    src/ox_sim.cpp
)

set(SIMULATOR_SOURCES
    src/driver.cpp
    ${SIMULATOR_CORE_SOURCES}
    ${GUI_SOURCES}
)

# Driver shared library - output as ox_driver.dll / libox_driver.so / libox_driver.dylib
add_library(ox-simulator SHARED ${SIMULATOR_SOURCES})

# Rename to standardized driver library name
set_target_properties(ox-simulator PROPERTIES
    OUTPUT_NAME "ox_driver"
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${OUTPUT_FOLDER}
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/${OUTPUT_FOLDER}
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/${OUTPUT_FOLDER}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${OUTPUT_FOLDER}
    LIBRARY_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/${OUTPUT_FOLDER}
    LIBRARY_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/${OUTPUT_FOLDER}
)

if(NOT WIN32)
    set_target_properties(ox-simulator PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
    )
endif()

# Include directories
target_include_directories(ox-simulator PUBLIC
    include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_include_directories(ox-simulator PRIVATE
    src
    ${GUI_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(ox-simulator PRIVATE spdlog::spdlog ${GUI_LIBRARIES})

# Platform-specific settings
if(WIN32)
elseif(APPLE)
    # Support macOS 10.13+ for broad compatibility
    set_target_properties(ox-simulator PROPERTIES
        OSX_DEPLOYMENT_TARGET "10.13"
    )
elseif(UNIX)
    # Linux requires dl library for dynamic loading
    target_link_libraries(ox-simulator PRIVATE ${CMAKE_DL_LIBS})
endif()

if(OX_SIM_BUILD_EXAMPLES)
    add_executable(ox_simulator_api_example
        examples/simulator_api_example.cpp
    )

    target_include_directories(ox_simulator_api_example PRIVATE
        include
        ${CMAKE_CURRENT_SOURCE_DIR}
    )

    target_link_libraries(ox_simulator_api_example PRIVATE ox-simulator spdlog::spdlog)

    set_target_properties(ox_simulator_api_example PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/examples
        RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/examples
        RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/examples
    )
endif()

if(BUILD_TESTING AND OX_SIM_BUILD_TESTS)
    add_subdirectory(tests)
endif()

# Print build completion message
add_custom_command(TARGET ox-simulator POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "====================================================================="
    COMMAND ${CMAKE_COMMAND} -E echo "ox simulator driver build complete!"
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "Output: ${CMAKE_BINARY_DIR}/${OUTPUT_FOLDER}"
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "Copy ${CMAKE_BINARY_DIR}/${OUTPUT_FOLDER} to your ox runtime's 'drivers' folder"
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "====================================================================="
    VERBATIM
)
