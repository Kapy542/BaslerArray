cmake_minimum_required(VERSION 3.10)
project(BaslerMultiCam)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(pylon REQUIRED)

add_executable(SynchronizedSnapshots SynchronizedSnapshots.cpp)

target_link_libraries(SynchronizedSnapshots pylon::pylon)