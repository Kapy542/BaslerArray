#pragma once

#include <pylon/PylonIncludes.h>
#include <pylon/BaslerUniversalInstantCamera.h>
#include <pylon/gige/GigETransportLayer.h>

#include <string>
#include <vector>

#include "configs/CameraConfig.h"

class CameraNode
{
public:
    CameraNode(Pylon::IPylonDevice* device, const std::string& id);
    ~CameraNode();

    void Configure(const CameraConfig& cfg);

    void ConfigureActionTrigger(uint32_t deviceKey, uint32_t groupKey, uint32_t groupMask);

    void EnablePTP();


    Pylon::CBaslerUniversalInstantCamera camera;
    std::string serial;
    std::string logicalId;
};

