#pragma once

#include <pylon/PylonIncludes.h>
#include <pylon/BaslerUniversalInstantCamera.h>

#include <string>
#include <vector>

#include "configs/CameraConfig.h"

class CameraNode
{
public:
    Pylon::CBaslerUniversalInstantCamera camera;
    std::string serial;
    std::string logicalId;

    CameraNode(Pylon::IPylonDevice* device, const std::string& id);
    ~CameraNode();

    void Configure(const CameraConfig& cfg);

    void ConfigureActionTrigger(uint32_t deviceKey, uint32_t groupKey, uint32_t groupMask);

    void EnablePTP();  

private: 
    bool TrySetEnum(GenApi::INodeMap& n, const std::string& name, const std::string& value);
    bool TrySetInt(GenApi::INodeMap& n, const std::string& name, uint32_t value);
    bool TrySetFloat(GenApi::INodeMap& n, const std::string& name, double value);
};

