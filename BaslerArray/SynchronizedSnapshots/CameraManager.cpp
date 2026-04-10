#include "CameraManager.h"

#include <pylon/PylonIncludes.h>
#include <pylon/BaslerUniversalInstantCamera.h>
#include <pylon/gige/GigETransportLayer.h>

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>

#include <vector>
#include <fstream>

#include "core/Frame.h"
#include "core/SafeQueue.h"
#include "utils/file_io.h"

#include <nlohmann/json.hpp>

using namespace Pylon;
using namespace GenApi;
using namespace std;
using json = nlohmann::json;

const int PREVIEW_EVERY_N = 1;

void Log(const string& msg) {
    cout << "[" << get_time_string() << "] " << msg << std::endl;
}


// ========================= CAMERA MANAGER =========================

CameraManager::CameraManager(const std::string& dir) : outputDir(dir) {}

map<string, string> CameraManager::LoadCameraOrder(const string& filename) {
    ifstream file(filename);
    if (!file.is_open()) {
        throw runtime_error("Failed to open camera order file: " + filename);
    }

    json j;
    file >> j;

    if (!j.is_object()) {
        throw runtime_error("Camera order JSON must be an object { \"01\": \"serial\", ... }");
    }

    map<string, string> order;

    for (const auto& item : j.items()) {
        const string& logicalId = item.key();

        if (!item.value().is_string()) {
            throw runtime_error("Invalid serial for camera " + logicalId);
        }

        string serial = item.value().get<string>();
        order[logicalId] = serial;
    }

    return order;
}

void CameraManager::DiscoverAndInit(const map<string, string>& order) {
    CTlFactory& factory = CTlFactory::GetInstance();
    DeviceInfoList_t devices;

    if (factory.EnumerateDevices(devices) == 0)
        throw runtime_error("No cameras found");

    for (auto& [id, serial] : order) {
        bool found = false;
        for (auto& dev : devices) {
            if (string(dev.GetSerialNumber()) == serial) {
                cameras.emplace_back(
                    make_unique<CameraNode>(factory.CreateDevice(dev), id));
                found = true;
                cout << "Connected to camera: " << id << " : " << serial << endl;
                break;
            }
        }
        if (!found)
            cerr << "Missing camera: " << serial << endl;
    }

    for (auto& cam : cameras) {
        cam->camera.Open();
        // cam->EnablePTP();
    }
    cout << "Connected to " << cameras.size() << " cameras" << endl << endl;
}

void CameraManager::ConfigureAll(const CameraConfig& cfg) {
    for (auto& cam : cameras) {
        cam->Configure(cfg);
    }

    cout << "Cameras configured." << endl << endl;
}

void CameraManager::SetupActionCommandTrigger() {
    const uint32_t deviceKey = 1;
    const uint32_t groupKey = 1;
    const uint32_t groupMask = 0xFFFFFFFF;

    for (auto& cam : cameras) {
        cam->ConfigureActionTrigger(deviceKey, groupKey, groupMask);
    }

    cout << "Action command trigger configured." << endl << endl;
}

void CameraManager::WaitForPtpSync() {
    cout << "Waiting for PTP synchronization..." << endl;

    for (auto& cam : cameras) {
        cam->EnablePTP();
    }

    bool allSynced = false;

    while (!allSynced) {
        allSynced = true;

        int master_count = 0;
        for (auto& cam : cameras) {
            INodeMap& n = cam->camera.GetNodeMap();

            // TODO: Does it really matter which one to use?
            //CCommandPtr(nodemap.GetNode("GevIEEE1588DataSetLatch"))->Execute();
            //auto status = CEnumerationPtr(n.GetNode("GevIEEE1588StatusLatched"))->ToString();

            auto status = CEnumerationPtr(n.GetNode("GevIEEE1588Status"))->ToString();

            cout << "Cam " << cam->logicalId << " PTP: " << status << endl;

            if (status == "Master") { master_count += 1; }

            if (status != "Master" && status != "Slave") {
                allSynced = false;
            }
        }
        if (master_count > 1) { allSynced = false; }

        this_thread::sleep_for(chrono::milliseconds(500));
    }

    // TODO: Wait for clocks to converge
    cout << "PTP synchronized across all cameras." << endl;
    for (auto& cam : cameras) {
        INodeMap& n = cam->camera.GetNodeMap();

        // TODO: Does it really matter which one to use?
        //CCommandPtr(nodemap.GetNode("GevIEEE1588DataSetLatch"))->Execute();
        //auto status = CEnumerationPtr(nodemap.GetNode("GevIEEE1588StatusLatched"))->ToString();

        auto status = CEnumerationPtr(n.GetNode("GevIEEE1588Status"))->ToString();

        int offsetFromMaster = CIntegerPtr(n.GetNode("GevIEEE1588OffsetFromMaster"))->GetValue();

        cout << "Cam " << cam->logicalId << " PTP: " << status << " Offset: " << offsetFromMaster << endl;
    }
}

void CameraManager::Start() {
    running = true;

    for (auto& cam : cameras)
        cam->camera.StartGrabbing(GrabStrategy_LatestImageOnly);

    // consumer thread
    consumerThread = thread(&CameraManager::ConsumeLoop, this);
    previewThread = thread(&CameraManager::PreviewLoop, this);

    // grab threads
    for (auto& cam : cameras)
        grabThreads.emplace_back(&CameraManager::GrabLoop, this, cam.get());
}

void CameraManager::Stop() {
    frameQueue.stop(); // Stop the queue so consumer won't get stuck
    previewQueue.stop();
    running = false;

    for (auto& cam : cameras) {
        if (cam->camera.IsGrabbing()) {
            cam->camera.StopGrabbing();
        }
    }

    for (auto& t : grabThreads) {
        if (t.joinable()) {
            t.join();
        }
    }

    if (consumerThread.joinable()) {
        consumerThread.join();
    }

    if (previewThread.joinable()) {
        previewThread.join();
    }
}

void CameraManager::FireActionCommand() {
    // Get the GigE transport layer.
    // We'll need it later to issue the action commands.
    CTlFactory& tlFactory = CTlFactory::GetInstance();
    IGigETransportLayer* pTL = dynamic_cast<IGigETransportLayer*>(tlFactory.CreateTl(BaslerGigEDeviceClass));

    std::cout << "Trigger cameras!" << std::endl;

    pTL->IssueActionCommand(
        1,              // device key
        1,              // group key
        0xFFFFFFFF      // group mask
    );

    cout << "Action command fired." << endl;
}

void CameraManager::GrabLoop(CameraNode* cam) {
    CGrabResultPtr res;

    while (running && cam->camera.IsGrabbing()) {
        //Log(cam->logicalId + " Waiting image...");
        if (cam->camera.RetrieveResult(50000, res, TimeoutHandling_ThrowException)) {

            if (res->GrabSucceeded()) {

                //Log("Got a image from: " + cam->logicalId);
                Frame f{
                    cam->logicalId,
                    res->GetTimeStamp(),
                    res->GetBlockID(),
                    res
                };

                // Always push to writer
                frameQueue.push(f);

                // Only every Nth frame goes to preview
                if (f.frameId % PREVIEW_EVERY_N == 0) {
                    previewQueue.push(f);
                }
            }
        }
    }
    Log("Grap loop for camera: " + cam->logicalId + " exiting...");
}

void CameraManager::ConsumeLoop() {
    while (running) {
        Frame f;

        //Log("Consume loop waiting data ");
        if (!frameQueue.pop(f)) {
            break; // queue stopped
        }

        Log("Writing " + f.cameraId + " Frame " + to_string(f.frameId) +
            " Timestamp " + to_string(f.timestamp) + "\n");

        SaveImage(f, outputDir);
    }

    cout << "Consumer thread exiting..." << endl;
}

// TODO: ?
void CameraManager::PreviewLoop() {
    while (running) {
        Frame f;

        if (!previewQueue.pop(f)) {
            break; // queue stopped
        }

        int w = f.grab->GetWidth();
        int h = f.grab->GetHeight();

        uint8_t* buffer = (uint8_t*)f.grab->GetBuffer();

        cv::Mat img(h, w, CV_8UC1, buffer);

        cv::imshow(f.cameraId, img);
        cv::waitKey(1);
    }
}
