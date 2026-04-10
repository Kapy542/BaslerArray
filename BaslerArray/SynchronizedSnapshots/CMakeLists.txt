cmake_minimum_required(VERSION 3.10)

project(BaslerMultiCam)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(pylon REQUIRED)

# If OPENCV_DIR is set (Windows), use it
if(DEFINED ENV{OPENCV_DIR})
    message(STATUS "Using OpenCV from OPENCV_DIR: $ENV{OPENCV_DIR}")
    set(OpenCV_DIR $ENV{OPENCV_DIR})
endif()

find_package(OpenCV REQUIRED)

message(STATUS "OpenCV version: ${OpenCV_VERSION}")

add_executable(SynchronizedSnapshots 
	SynchronizedSnapshots.cpp 
	CameraManager.cpp
	CameraNode.cpp
	configs/config_loader.cpp
	utils/file_io.cpp
)

target_include_directories(SynchronizedSnapshots PRIVATE 
	${CMAKE_SOURCE_DIR}/../Dependencies
)
	
target_link_libraries(SynchronizedSnapshots 
	pylon::pylon
	${OpenCV_LIBS}
)
