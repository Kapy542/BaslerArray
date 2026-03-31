#include "CameraNode.h"
#include <iostream>

using namespace Pylon;
using namespace GenApi;
using namespace std;

CameraNode::CameraNode(IPylonDevice* device, const string& id)
    : camera(device), logicalId(id) {
    serial = camera.GetDeviceInfo().GetSerialNumber();
}

CameraNode::~CameraNode() {
    std::cout << "Closing camera " << logicalId << endl;
    if (camera.IsOpen())
        camera.Close();
}

void CameraNode::Configure(const CameraConfig& cfg) {
    INodeMap& n = camera.GetNodeMap();

    TrySetEnum(n, "PixelFormat", "BayerRG8");

    TrySetEnum(n, "BalanceWhiteAuto", "Off");
    TrySetEnum(n, "ExposureAuto", "Off");
    TrySetEnum(n, "GainAuto", "Off");

    if (cfg.width > 0)
        TrySetInt(n, "Width", cfg.width);

    if (cfg.height > 0)
        TrySetInt(n, "Height", cfg.height);

    if (cfg.exposure > 0)
        TrySetFloat(n, "ExposureTimeAbs", cfg.exposure);

    if (cfg.gain > 0)
        TrySetFloat(n, "GainRaw", cfg.gain);

    TrySetEnum(n, "LightSourceSelector", cfg.lightSourceSelector);

    for (const auto& br : cfg.balanceRatios) {
        TrySetEnum(n, "BalanceRatioSelector", br.selector);
        TrySetInt(n, "BalanceRatioRaw", br.balanceRatioRaw);
    }

    TrySetInt(n, "GevSCPSPacketSize", 1500);

    cout << "Configured camera " << logicalId << endl;
}

void CameraNode::ConfigureActionTrigger(uint32_t deviceKey, uint32_t groupKey, uint32_t groupMask) {
    INodeMap& n = camera.GetNodeMap();

    // Enable trigger mode
    TrySetEnum(n, "TriggerSelector", "FrameStart");
    TrySetEnum(n, "TriggerMode", "On");
    TrySetEnum(n, "TriggerSource", "Action1");

    // Continuous acquisition (required)
    TrySetEnum(n, "AcquisitionMode", "Continuous");

    // Configure action keys
    TrySetInt(n, "ActionDeviceKey", deviceKey);
    TrySetInt(n, "ActionGroupKey", groupKey);
    TrySetInt(n, "ActionGroupMask", groupMask);
}

void CameraNode::EnablePTP() {
    INodeMap& n = camera.GetNodeMap();
    if (IsWritable(n.GetNode("PtpEnable"))) {
        cout << "Set PtpEnable" << endl;
        CBooleanPtr(n.GetNode("PtpEnable"))->SetValue(true);
    }
    if (IsWritable(n.GetNode("GevIEEE1588"))) {
        cout << "Set GevIEEE1588" << endl;
        CBooleanPtr(n.GetNode("GevIEEE1588"))->SetValue(true);
    }
}



// ========================= HELPER FUNCTIONS =========================

bool CameraNode::TrySetEnum(INodeMap& n, const string& name, const string& value) {
    try {
        CEnumerationPtr node(n.GetNode(name.c_str()));
        if (!node || !IsWritable(node)) {
            cerr << "[WARN] " << name << " not writable\n";
            return false;
        }

        node->FromString(value.c_str());
        return true;
    }
    catch (const GenericException& e) {
        cerr << "[ERROR] " << name << ": " << e.GetDescription() << endl;
        return false;
    }
}

bool CameraNode::TrySetInt(INodeMap& n, const string& name, uint32_t value) {
    try {
        CIntegerPtr node(n.GetNode(name.c_str()));
        if (!node || !IsWritable(node)) {
            cerr << "[WARN] " << name << " not writable\n";
            return false;
        }

        node->SetValue(value);
        return true;
    }
    catch (const GenericException& e) {
        cerr << "[ERROR] " << name << ": " << e.GetDescription() << endl;
        return false;
    }
}

bool CameraNode::TrySetFloat(INodeMap& n, const string& name, double value) {
    try {
        CFloatPtr node(n.GetNode(name.c_str()));
        if (!node || !IsWritable(node)) {
            cerr << "[WARN] " << name << " not writable\n";
            return false;
        }

        node->SetValue(value);
        return true;
    }
    catch (const GenericException& e) {
        cerr << "[ERROR] " << name << ": " << e.GetDescription() << endl;
        return false;
    }
}