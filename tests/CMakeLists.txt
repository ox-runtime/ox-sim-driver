include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest
    GIT_TAG v1.16.0
    GIT_SHALLOW true
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(ox_sim_tests
    test_lifecycle.cpp
    test_profiles.cpp
    test_devices.cpp
    test_inputs.cpp
    test_frame.cpp
    ../src/ox_sim.cpp
)

target_include_directories(ox_sim_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${OX_DRIVER_INCLUDE_DIR}
)

target_link_libraries(ox_sim_tests PRIVATE GTest::gtest GTest::gtest_main)

set_target_properties(ox_sim_tests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/tests
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin/tests
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin/tests
)

enable_testing()
add_test(NAME ox_sim_tests COMMAND ox_sim_tests)