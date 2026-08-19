#pragma once

#include <pylon/PylonIncludes.h>
#include <string>

// ========================= FRAME =========================
struct Frame {
    std::string cameraId;
    uint64_t timestamp;
    uint64_t frameId;
    Pylon::CGrabResultPtr grab;
};
