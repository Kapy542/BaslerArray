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
#include "utils/Log.h"

#include <nlohmann/json.hpp>

using namespace Pylon;
using namespace GenApi;
using namespace std;
using json = nlohmann::json;

const int PREVIEW_EVERY_N = 10;
const int FPS = 10;
const int period = 1000 / FPS;
/*
void Log(const string& msg) {
    std::cout << "[" << get_time_string() << "] " << msg << std::endl;
}
*/

// ========================= CAMERA MANAGER =========================

// ============================= SETUP ==============================

CameraManager::CameraManager(const std::string& dir) : outputDir(dir) {}
CameraManager::~CameraManager() {
    Stop();
}

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
                std::cout << "Connected to camera: " << id << " : " << serial << std::endl;
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
    std::cout << "Connected to " << cameras.size() << " cameras" << std::endl << std::endl;
}

void CameraManager::ConfigureAll(const CameraConfig& cfg) {
    for (auto& cam : cameras) {
        cam->Configure(cfg);
    }

    std::cout << "Cameras configured." << std::endl << std::endl;
}

void CameraManager::SetupActionCommandTrigger() {
    const uint32_t deviceKey = 1;
    const uint32_t groupKey = 1;
    const uint32_t groupMask = 0xFFFFFFFF;

    for (auto& cam : cameras) {
        cam->ConfigureActionTrigger(deviceKey, groupKey, groupMask);
    }

    std::cout << "Action command trigger configured." << std::endl << std::endl;
}

// TODO: this!
void CameraManager::WaitForPtpSync() {
    std::cout << "Waiting for PTP synchronization..." << std::endl;

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

            std::cout << "Cam " << cam->logicalId << " PTP: " << status << std::endl;

            if (status == "Master") { master_count += 1; }

            if (status != "Master" && status != "Slave") {
                allSynced = false;
            }
        }
        if (master_count > 1) { allSynced = false; }

        this_thread::sleep_for(chrono::milliseconds(500));
    }

    // TODO: Wait for clocks to converge
    std::cout << "PTP synchronized across all cameras." << std::endl;
    for (auto& cam : cameras) {
        INodeMap& n = cam->camera.GetNodeMap();

        // TODO: Does it really matter which one to use?
        //CCommandPtr(nodemap.GetNode("GevIEEE1588DataSetLatch"))->Execute();
        //auto status = CEnumerationPtr(nodemap.GetNode("GevIEEE1588StatusLatched"))->ToString();

        auto status = CEnumerationPtr(n.GetNode("GevIEEE1588Status"))->ToString();

        int offsetFromMaster = CIntegerPtr(n.GetNode("GevIEEE1588OffsetFromMaster"))->GetValue();

        std::cout << "Cam " << cam->logicalId << " PTP: " << status << " Offset: " << offsetFromMaster << std::endl;
    }
}

void CameraManager::Start() {
    running = true;

    for (auto& cam : cameras)
        cam->camera.StartGrabbing(GrabStrategy_LatestImageOnly);

    // Consumer threads (Write and preview)
    consumerThread = thread(&CameraManager::ConsumeLoop, this);
    previewThread = thread(&CameraManager::PreviewLoop, this);

    // Threads retrieving images from the cameras and assigning them onto queue
    for (auto& cam : cameras)
        grabThreads.emplace_back(&CameraManager::GrabLoop, this, cam.get());

    // Trigger thread
    triggerThread = thread(&CameraManager::TriggerLoop, this);
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

    if (triggerThread.joinable()) {
        triggerThread.join();
    }
}


// ============================ COMMANDS ============================


void CameraManager::FireActionCommand() {
    // Get the GigE transport layer.
    // We'll need it later to issue the action commands.
    CTlFactory& tlFactory = CTlFactory::GetInstance();
    IGigETransportLayer* pTL = dynamic_cast<IGigETransportLayer*>(tlFactory.CreateTl(BaslerGigEDeviceClass));

    //std::cout << "Trigger cameras!" << std::endl;

    // Issue action command to all interfaces
    pTL->IssueActionCommand(
        1,              // device key
        1,              // group key
        0xFFFFFFFF,     // group mask
        "255.255.255.255"
    );

    //cout << "Action command fired." << endl;
}

bool CameraManager::IsRunning() const {
    return running.load();
}

void CameraManager::RequestSave() {
    std::cout << "Save requested!" << std::endl;
    saveTriggerId = triggerId.load() + 1;
}

void CameraManager::StartRecording() {
    recording = true;
    std::cout << "Recording started" << std::endl;
}

void CameraManager::StopRecording() {
    recording = false;
    std::cout << "Recording stopped" << std::endl;
}

// ============================ THREADS =============================


void CameraManager::TriggerLoop() {
    while (running) {
        FireActionCommand();
        std::this_thread::sleep_for(std::chrono::milliseconds(period));
        triggerId++;
    }
}

void CameraManager::GrabLoop(CameraNode* cam) {
    CGrabResultPtr res;

    while (running && cam->camera.IsGrabbing()) {
        //Log(cam->logicalId + " Waiting image...");
        if (cam->camera.RetrieveResult(5000, res, TimeoutHandling_ThrowException)) {

            if (res->GrabSucceeded()) {

                //Log("Got a image from: " + cam->logicalId);
                /*
                if (cam->logicalId == "01") {
                    Log("Got a image from: " + cam->logicalId);
                    Log("BlockID: " + std::to_string(res->GetBlockID()) + 
                        "  triggerID: " + std::to_string(triggerId.load()) + 
                        "  saveTriggerID: " + std::to_string(saveTriggerId.load()));
                }
                */

                Frame f{
                    cam->logicalId,
                    res->GetTimeStamp(),
                    res->GetBlockID(),
                    res
                };

                
                // A single image with requested index is written to the disk
                // res->GetBlockID() should match corresponding triggerId?
                if (res->GetBlockID() == saveTriggerId.load()) {
                    //Log(cam->logicalId + " found matching ID...");
                    frameQueue.push(f);
                }
                

                if (recording) {
                    frameQueue.push(f);
                }

                // Every Nth frame goes to preview
                if (f.frameId % PREVIEW_EVERY_N == 0) {
                    previewQueue.push(f);
                }
            }
            else
            {
                std::cerr << "Grab failed. "
                    << "Error code: " << res->GetErrorCode()
                    << ", Description: " << res->GetErrorDescription()
                    << std::endl;
            }
        }
    }
    Log("Grap loop for camera: " + cam->logicalId + " exiting...");
}

void CameraManager::ConsumeLoop() {
    Frame f;

    // Even if stopped, goes through whole queue before exiting
    while (frameQueue.pop(f)) {
        /*
        Log("Writing " + f.cameraId + " Frame " + to_string(f.frameId) +
            " Timestamp " + to_string(f.timestamp) + "\n");
        */
        if (f.frameId % 10 == 0)
        {
            Log("Queue size: " + std::to_string(frameQueue.size()));
        }
        if (recording) {
            SaveRaw(f, outputDir);
        }
        else {
            SaveImage(f, outputDir);
        }
    }

    std::cout << "Consumer thread exiting..." <<std:: endl;
}

void CameraManager::PreviewLoop() {
    std::map<uint64_t, std::map<std::string, Frame>> buffer;

    int numCameras = cameras.size();
    int maxWidth = 1920;
    int maxHeight = 1200;

    while (running) {
        Frame f;

        // Stop immediately, do not process any remaining frames
        if (!previewQueue.pop(f)) {
            break;
        }

        buffer[f.frameId][f.cameraId] = f;

        if (buffer[f.frameId].size() == numCameras) {

            auto& frames = buffer[f.frameId];

            int w = frames.begin()->second.grab->GetWidth();
            int h = frames.begin()->second.grab->GetHeight();

            // TODO: Automatic grid size
            int cols = 2;
            int rows = 3;

            cv::Mat grid = cv::Mat::zeros(rows * h, cols * w, CV_8UC3);

            int i = 0;
            for (auto& [id, frame] : frames) {

                cv::Mat img(h, w, CV_8UC1, (uint8_t*)frame.grab->GetBuffer());

                // Bayer to color
                cv::Mat imgColor;
                cv::cvtColor(img, imgColor, cv::COLOR_BayerRG2RGB);

                // Camera_idx
                cv::putText(
                    imgColor,
                    "Cam " + frame.cameraId,
                    cv::Point(30, 50),              // position
                    cv::FONT_HERSHEY_SIMPLEX,
                    2.0,                            // font scale
                    cv::Scalar(0, 255, 0),          // green text
                    3                               // thickness
                );

                // Timestamp
                cv::putText(imgColor,
                    "Frame " + std::to_string(frame.frameId),
                    cv::Point(35, 100),
                    cv::FONT_HERSHEY_SIMPLEX,
                    1.3,
                    cv::Scalar(255, 255, 0),
                    2);

                int r = i / cols;
                int c = i % cols;
                imgColor.copyTo(grid(cv::Rect(c * w, r * h, w, h)));

                i++;
            }

            // Resize to fit the display
            cv::Mat display = grid;

            double scale = std::min(
                (double)maxWidth / grid.cols,
                (double)maxHeight / grid.rows
            );

            if (scale < 1.0) {
                cv::resize(grid, display, cv::Size(), scale, scale);
            }

            cv::imshow("Preview   w: write   r: start   t: stop   ESC/q: exit", display);
            int key = cv::waitKey(1);

            if (key == 'q' || key == 27) { // ESC
                std::cout << "Exit requested..." << std::endl;
                running = false;
            }
            else if (key == 'w') {
                RequestSave();              
            }
            else if (key == 'r') {
                StartRecording();
            }
            else if (key == 't') {
                StopRecording();
            }

            buffer.erase(f.frameId);
        }

        // prevent memory growth
        if (buffer.size() > 50) {
            buffer.erase(buffer.begin());
        }
    }

    cv::destroyAllWindows();

    std::cout << "Preview thread exiting..." << std::endl;
}