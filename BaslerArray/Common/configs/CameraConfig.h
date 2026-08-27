#pragma once

#include <string>
#include <vector>

struct WhiteBalanceConfig
{
    std::string mode;
    int red;
    int green;
    int blue;
};

struct CameraConfig
{
    std::string serialNumber;

    int width;
    int height;
    std::string pixelFormat;

    bool reverseX;
    bool reverseY;

    double exposureUs;
    double gain;
    std::string exposureAuto;
    std::string gainAuto;

    WhiteBalanceConfig whiteBalance;

    double fps;
    int packetSize;
};