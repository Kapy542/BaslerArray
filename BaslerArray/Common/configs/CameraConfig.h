#pragma once

#include <string>
#include <vector>

struct WhiteBalanceConfig
{
    std::string mode;
    std::string lightSource;
    int red;
    int green;
    int blue;
};

struct CameraConfig
{
    std::string serialNumber;

    int width;
    int height;
    //std::string pixelFormat;

    bool reverseX;
    bool reverseY;

    double exposureUs;
    double gainRaw;
    std::string exposureAuto;
    std::string gainAuto;

    WhiteBalanceConfig whiteBalance;

    double fps;
    int packetSize;
};