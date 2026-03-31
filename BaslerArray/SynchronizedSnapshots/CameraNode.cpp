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
    // TODO: FPS and delay?
    INodeMap& n = camera.GetNodeMap();
    try {
        CEnumerationPtr(n.GetNode("PixelFormat"))->FromString("BayerRG8");

        CEnumerationPtr(n.GetNode("BalanceWhiteAuto"))->FromString("Off");
        CEnumerationPtr(n.GetNode("ExposureAuto"))->FromString("Off");
        CEnumerationPtr(n.GetNode("GainAuto"))->FromString("Off");

        if (cfg.width > 0 && IsWritable(n.GetNode("Width")))
            CIntegerPtr(n.GetNode("Width"))->SetValue(cfg.width);

        if (cfg.height > 0 && IsWritable(n.GetNode("Height")))
            CIntegerPtr(n.GetNode("Height"))->SetValue(cfg.height);

        if (cfg.exposure > 0 && IsWritable(n.GetNode("ExposureTime")))
            CFloatPtr(n.GetNode("ExposureTime"))->SetValue(cfg.exposure);

        if (cfg.gain > 0 && IsWritable(n.GetNode("Gain")))
            CFloatPtr(n.GetNode("Gain"))->SetValue(cfg.gain);

        // Light source
        CEnumerationPtr(n.GetNode("LightSourceSelector"))
            ->FromString(cfg.lightSourceSelector.c_str());

        // White balance ratios
        for (const auto& br : cfg.balanceRatios) {
            CEnumerationPtr(n.GetNode("BalanceRatioSelector"))
                ->FromString(br.selector.c_str());

            CIntegerPtr(n.GetNode("BalanceRatioRaw"))
                ->SetValue(br.balanceRatioRaw);
        }

        //CIntegerPtr(n.GetNode("GevSCPSPacketSize"))->SetValue(9000);

        std::cout << "Configured camera " << logicalId << endl;
    }

    catch (const GenericException& e) {
        cerr << "Config error: " << e.GetDescription() << endl;
    }
}

void CameraNode::ConfigureActionTrigger(uint32_t deviceKey, uint32_t groupKey, uint32_t groupMask) {
    INodeMap& n = camera.GetNodeMap();

    // Enable trigger mode
    CEnumerationPtr(n.GetNode("TriggerSelector"))->FromString("FrameStart");
    CEnumerationPtr(n.GetNode("TriggerMode"))->FromString("On");
    CEnumerationPtr(n.GetNode("TriggerSource"))->FromString("Action1");

    // Continuous acquisition (required)
    CEnumerationPtr(n.GetNode("AcquisitionMode"))->FromString("Continuous");

    // Configure action keys
    CIntegerPtr(n.GetNode("ActionDeviceKey"))->SetValue(deviceKey);
    CIntegerPtr(n.GetNode("ActionGroupKey"))->SetValue(groupKey);
    CIntegerPtr(n.GetNode("ActionGroupMask"))->SetValue(groupMask);
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
