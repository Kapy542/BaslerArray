cmake_minimum_required(VERSION 3.16)

project(BaslerArray)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(pylon REQUIRED)
find_package(OpenCV REQUIRED)

add_subdirectory(Common)
add_subdirectory(PTP_Recorder)