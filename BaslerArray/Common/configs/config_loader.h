#pragma once

#include <string>
#include "CameraConfig.h"

std::map<std::string, std::string> LoadCameraMapping(const std::string& filename);

CameraConfig LoadCameraConfig(const std::string& filename);

std::map<std::string, CameraConfig> BuildCameraConfigs(
    const std::map<std::string, std::string>& cameraMapping,
    const CameraConfig& defaultConfig);