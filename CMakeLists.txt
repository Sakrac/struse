cmake_minimum_required(VERSION 3.16)
project(struse_test CXX)

enable_testing()

add_executable(struse_test struse_test.cpp)
target_include_directories(struse_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

set_target_properties(struse_test PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
)

add_test(NAME struse_smoke COMMAND struse_test)
