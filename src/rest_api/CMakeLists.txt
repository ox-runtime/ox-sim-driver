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

FetchContent_MakeAvailable(asio)
set(ASIO_INCLUDE_DIR ${asio_SOURCE_DIR}/asio/include)

FetchContent_MakeAvailable(crow)

FetchContent_MakeAvailable(stb)

set(REST_API_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/http_server.cpp
    PARENT_SCOPE
)

set(REST_API_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${crow_SOURCE_DIR}/include
    ${ASIO_INCLUDE_DIR}
    ${stb_SOURCE_DIR}
    PARENT_SCOPE
)

set(REST_API_COMPILE_DEFINITIONS
    ASIO_STANDALONE
    PARENT_SCOPE
)