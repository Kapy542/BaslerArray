#pragma once

#include <string>
#include <vector>

struct BalanceRatio {
    std::string selector;
    int balanceRatioRaw;
};

struct CameraConfig {
    int width = 0;
    int height = 0;
    double exposure = 0.0;
    double gain = 0.0;

    std::string lightSourceSelector;

    struct BalanceRatio {
        std::string selector;
        int balanceRatioRaw;
    };

    std::vector<BalanceRatio> balanceRatios;
};