cmake_minimum_required(VERSION 3.15)
project(ox-simulator VERSION 0.3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(OUTPUT_FOLDER "ox_simulator")

# Prefer GLVND on Linux so FindOpenGL resolves the split libOpenGL/libGLX packages
# used on modern distributions and GitHub runners.
if(UNIX AND NOT APPLE)
    set(OpenGL_GL_PREFERENCE GLVND)
endif()

# Add subdirectories
add_subdirectory(src/api)
add_subdirectory(src/gui)

# Source files
set(SIMULATOR_SOURCES
    src/driver.cpp
    src/simulator_core.cpp
    src/device_profiles.cpp
    ${API_SOURCES}
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

# Include directories
target_include_directories(ox-simulator PRIVATE
    src
    ${CMAKE_SOURCE_DIR}/src/api
    ${CMAKE_SOURCE_DIR}/src/api/third_party
    ${GUI_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}  # For ox_driver.h in root directory
)

# Link libraries
target_link_libraries(ox-simulator PRIVATE ${GUI_LIBRARIES})

# Platform-specific settings
if(WIN32)
    # Windows requires ws2_32 for networking and _WIN32_WINNT definition for ASIO
    target_link_libraries(ox-simulator PRIVATE ws2_32)
    target_compile_definitions(ox-simulator PRIVATE _WIN32_WINNT=0x0601)  # Windows 7+
elseif(APPLE)
    # Support macOS 10.13+ for broad compatibility
    set_target_properties(ox-simulator PROPERTIES
        OSX_DEPLOYMENT_TARGET "10.13"
    )
elseif(UNIX)
    # Linux requires dl library for dynamic loading
    target_link_libraries(ox-simulator PRIVATE ${CMAKE_DL_LIBS})
endif()

# Copy config.json to output directory
add_custom_command(TARGET ox-simulator POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/config.json
        ${CMAKE_BINARY_DIR}/${OUTPUT_FOLDER}/config.json
    COMMENT "Copying config.json to output directory"
)

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
