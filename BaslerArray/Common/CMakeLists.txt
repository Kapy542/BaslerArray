add_library(Common STATIC
    CameraManager.cpp
    CameraNode.cpp

    configs/config_loader.cpp

    utils/file_io.cpp
    utils/Log.cpp
)

target_include_directories(Common
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/configs
        ${CMAKE_CURRENT_SOURCE_DIR}/utils
        ${CMAKE_SOURCE_DIR}/Dependencies
)

target_link_libraries(Common
    PUBLIC
        pylon::pylon
        ${OpenCV_LIBS}
)