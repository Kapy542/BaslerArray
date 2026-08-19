#include "config_loader.h"
#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

CameraConfig LoadConfig(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open())
        throw std::runtime_error("Failed to open config file");

    json j;
    file >> j;

    CameraConfig cfg;

    cfg.width = j.value("width", 0);
    cfg.height = j.value("height", 0);
    cfg.exposure = j.value("exposure", 0.0);
    cfg.gain = j.value("gain", 0.0);

    cfg.lightSourceSelector = j.value("lightSourceSelector", "");

    if (j.contains("balanceRatioSelector")) {
        for (const auto& item : j["balanceRatioSelector"]) {
            cfg.balanceRatios.push_back({
                item.value("selector", ""),
                item.value("balanceRatioRaw", 0)
                });
        }
    }

    return cfg;
}