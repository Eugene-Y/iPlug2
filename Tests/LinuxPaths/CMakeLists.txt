cmake_minimum_required(VERSION 3.20)

project(IPlugLinuxPathsTests LANGUAGES CXX)

enable_testing()

add_executable(iplug-linux-paths-test IPlugLinuxPathsTest.cpp)
target_compile_features(iplug-linux-paths-test PRIVATE cxx_std_17)
target_include_directories(iplug-linux-paths-test PRIVATE ../..)

add_test(NAME iplug-linux-paths COMMAND iplug-linux-paths-test)
