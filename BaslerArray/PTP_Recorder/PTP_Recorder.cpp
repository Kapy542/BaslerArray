// Basler Multi-Camera Capture Template
// Requirements:
// - Pylon SDK 26.03
// - C++17
// - nlohmann/json

#include <pylon/PylonIncludes.h>

/*
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>
*/

#include <iostream>
#include <map>
#include <string>

#include "CameraNode.h"
#include "CameraManager.h"
#include "utils/file_io.h"
#include "configs/CameraConfig.h"
#include "configs/config_loader.h"

using namespace Pylon;
using namespace std;

std::string out_folder = "./recordings/";

int main()
{
    std::cout << "Hello World!\n";
}

