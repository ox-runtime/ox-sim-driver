include(FetchContent)

FetchContent_Declare(
    crow
    GIT_REPOSITORY https://github.com/CrowCpp/Crow.git
    GIT_TAG v1.3.1
    GIT_SHALLOW true
)

FetchContent_Declare(
    asio
    GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
    GIT_TAG asio-1-34-2
    GIT_SHALLOW true
)

FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG master
    GIT_SHALLOW true
)

FetchContent_GetProperties(crow)
if(NOT crow_POPULATED)
    FetchContent_Populate(crow)
endif()

FetchContent_GetProperties(asio)
if(NOT asio_POPULATED)
    FetchContent_Populate(asio)
endif()

FetchContent_GetProperties(stb)
if(NOT stb_POPULATED)
    FetchContent_Populate(stb)
endif()

set(REST_API_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/http_server.cpp
    PARENT_SCOPE
)

set(REST_API_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${crow_SOURCE_DIR}/include
    ${asio_SOURCE_DIR}/asio/include
    ${stb_SOURCE_DIR}
    PARENT_SCOPE
)

set(REST_API_COMPILE_DEFINITIONS
    ASIO_STANDALONE
    PARENT_SCOPE
)