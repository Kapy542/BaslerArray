#include "config_loader.h"

#include <fstream>
#include <iostream>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

std::map<std::string, std::string> LoadCameraMapping(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open camera order file: " + filename);
    }

    json j;
    file >> j;

    if (!j.is_object()) {
        throw std::runtime_error("Camera order JSON must be an object { \"01\": \"serial\", ... }");
    }

    std::map<std::string, std::string> mapping;

    for (const auto& item : j.items()) {
        const std::string& logicalId = item.key();

        if (!item.value().is_string()) {
            throw std::runtime_error("Invalid serial for camera " + logicalId);
        }

        std::string serial = item.value().get<std::string>();
        mapping[logicalId] = serial;
    }

    return mapping;
}

CameraConfig LoadCameraConfig(const std::string& filename) {
    std::ifstream file(filename);

    if (!file.is_open())
    {
        throw std::runtime_error("Failed to open camera config: " + filename);
    }

    nlohmann::json j;
    file >> j;

    CameraConfig config;

    config.width = j["width"];
    config.height = j["height"];
    config.pixelFormat = j["pixelFormat"];

    config.reverseX = j["reverseX"];
    config.reverseY = j["reverseY"];

    config.exposureUs = j["exposureUs"];
    config.gain = j["gain"];
    config.exposureAuto = j["exposureAuto"];
    config.gainAuto = j["gainAuto"];

    config.whiteBalance.mode = j["whiteBalance"]["mode"];
    config.whiteBalance.red = j["whiteBalance"]["red"];
    config.whiteBalance.green = j["whiteBalance"]["green"];
    config.whiteBalance.blue = j["whiteBalance"]["blue"];

    config.fps = j["fps"];
    config.packetSize = j["packetSize"];

    return config;
}

std::map<std::string, CameraConfig> BuildCameraConfigs(
    const std::map<std::string, std::string>& cameraMapping,
    const CameraConfig& defaultConfig)
{
    std::map<std::string, CameraConfig> cameraConfigs;

    for (const auto& [cameraName, serialNumber] : cameraMapping)
    {
        // Start with the default configuration
        CameraConfig config = defaultConfig;

        // Store the physical camera serial number
        config.serialNumber = serialNumber;

        // Look for camera-specific configuration
        std::string filename = "configs/cameras/" + cameraName + ".json";

        std::ifstream file(filename);

        if (file.is_open())
        {
            std::cout << "Found config file for camera: " << cameraName << std::endl;

            nlohmann::json json;
            file >> json;

            // Override only values that are present
            if (json.contains("width"))
                config.width = json["width"];

            if (json.contains("height"))
                config.height = json["height"];

            if (json.contains("pixelFormat"))
                config.pixelFormat = json["pixelFormat"];

            if (json.contains("reverseX"))
                config.reverseX = json["reverseX"];

            if (json.contains("reverseY"))
                config.reverseY = json["reverseY"];

            if (json.contains("exposureUs"))
                config.exposureUs = json["exposureUs"];

            if (json.contains("gain"))
                config.gain = json["gain"];

            if (json.contains("exposureAuto"))
                config.exposureAuto = json["exposureAuto"];

            if (json.contains("gainAuto"))
                config.gainAuto = json["gainAuto"];

            if (json.contains("whiteBalance"))
            {
                const auto& wb = json["whiteBalance"];

                if (wb.contains("mode"))
                    config.whiteBalance.mode = wb["mode"];

                if (wb.contains("red"))
                    config.whiteBalance.red = wb["red"];

                if (wb.contains("green"))
                    config.whiteBalance.green = wb["green"];

                if (wb.contains("blue"))
                    config.whiteBalance.blue = wb["blue"];
            }

            if (json.contains("fps"))
                config.fps = json["fps"];

            if (json.contains("packetSize"))
                config.packetSize = json["packetSize"];
        }

        cameraConfigs[cameraName] = config;
    }

    return cameraConfigs;
}