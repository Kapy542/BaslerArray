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
    try {
        if (camera.IsGrabbing())
            camera.StopGrabbing();

        if (camera.IsOpen())
            camera.Close();
    }
    catch (const exception& e) { cerr << "Error in CameraNode destructor: " << e.what() << endl; }
}

void CameraNode::Configure(const CameraConfig& config) {
    INodeMap& n = camera.GetNodeMap();

    cameraConfiguration = config;

    // Pixel format
    // Before setting, use default orientation because mirroring the image will change the bayer pattern order
    TrySetBool(n, "ReverseX", false);
    TrySetBool(n, "ReverseY", false);
    //TrySetEnum(n, "PixelFormat", config.pixelFormat);
    TrySetEnum(n, "PixelFormat", "BayerRG8");

    // Image dimensions
    TrySetInt(n, "Width", config.width);
    TrySetInt(n, "Height", config.height);

    // Image orientation
    TrySetBool(n, "ReverseX", config.reverseX);
    TrySetBool(n, "ReverseY", config.reverseY);

    // Exposure
    TrySetEnum(n, "ExposureAuto", config.exposureAuto);
    TrySetFloat(n, "ExposureTimeAbs", config.exposureUs);

    // Gain
    TrySetEnum(n, "GainAuto", config.gainAuto);
    TrySetInt(n, "GainRaw", config.gainRaw);

    // White balance
    TrySetEnum(n, "BalanceWhiteAuto", config.whiteBalance.mode);
    TrySetEnum(n, "LightSourceSelector", config.whiteBalance.lightSource);

    TrySetEnum(n, "BalanceRatioSelector", "Red");
    TrySetInt(n, "BalanceRatioRaw", config.whiteBalance.red);

    TrySetEnum(n, "BalanceRatioSelector", "Green");
    TrySetInt(n, "BalanceRatioRaw", config.whiteBalance.green);

    TrySetEnum(n, "BalanceRatioSelector", "Blue");
    TrySetInt(n, "BalanceRatioRaw", config.whiteBalance.blue);

    // Frame rate
    TrySetBool(n, "AcquisitionFrameRateEnable", false);
    //TrySetFloat(n, "AcquisitionFrameRateAbs", config.fps);

    // GigE packet size
    TrySetInt(n, "GevSCPSPacketSize", config.packetSize);

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

void CameraNode::ConfigureSynchronousFreeRun() {
    double fps = cameraConfiguration.fps;

    INodeMap& n = camera.GetNodeMap();

    TrySetEnum(n, "AcquisitionMode", "Continuous");
    TrySetEnum(n, "TriggerSelector", "FrameStart");
    TrySetEnum(n, "TriggerMode", "Off");

    // Let the free run start immediately without a specific start time
    TrySetInt(n, "SyncFreeRunTimerStartTimeLow", 0);
    TrySetInt(n, "SyncFreeRunTimerStartTimeHigh", 0);

    // Specify a trigger rate
    TrySetFloat(n, "SyncFreeRunTimerTriggerRateAbs", fps);

    // Apply the changes
    TryExecuteCommand(n, "SyncFreeRunTimerUpdate");

    // Enable Synchronous Free Run
    TrySetBool(n, "SyncFreeRunTimerEnable", true);
}

// TODO: this!
void CameraNode::EnablePTP() {
    INodeMap& n = camera.GetNodeMap();

    TrySetBool(n, "GevIEEE1588", true);
}



// ========================= HELPER FUNCTIONS =========================

bool CameraNode::TrySetEnum(INodeMap& n, const string& name, const string& value) {
    //cout << "Setting " << name << " to " << value << endl;
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
    //cout << "Setting " << name << " to " << value << endl;
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
    //cout << "Setting " << name << " to " << value << endl;
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

bool CameraNode::TrySetBool(INodeMap & n, const string & name, bool value) {
    //cout << "Setting " << name << " to " << value << endl;
    try {
        CBooleanPtr node(n.GetNode(name.c_str()));
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

bool CameraNode::TryExecuteCommand(INodeMap& n, const string& name) {
    //cout << "Executing command " << name << endl;
    try {
        CCommandPtr node(n.GetNode(name.c_str()));
        node->Execute();
        return true;
    }
    catch (const GenericException& e) {
        cerr << "[ERROR] " << name << ": " << e.GetDescription() << endl;
        return false;
    }
}