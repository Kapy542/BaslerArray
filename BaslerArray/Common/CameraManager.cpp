#include "CameraManager.h"
/*
#include <pylon/PylonIncludes.h>
#include <pylon/BaslerUniversalInstantCamera.h>
#include <pylon/gige/GigETransportLayer.h>
*/
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>

#include <vector>
#include <fstream>
#include <cmath>

#include "core/SafeQueue.h"
#include "utils/file_io.h"
#include "utils/Log.h"

#include <nlohmann/json.hpp>

using namespace Pylon;
using namespace GenApi;
using namespace std;
using json = nlohmann::json;

//const int PREVIEW_EVERY_N = 2;
//const int FPS = 2;
//const int period = 1000 / FPS;

/*
void Log(const string& msg) {
    std::cout << "[" << get_time_string() << "] " << msg << std::endl;
}
*/

// ========================= CAMERA MANAGER =========================

// ============================= SETUP ==============================

CameraManager::CameraManager() {}

CameraManager::~CameraManager() {
    Stop();
}

void CameraManager::Initialize(
        const std::map<std::string, std::string>& cameraMapping, 
        const map<string, CameraConfig>& cameraConfigs,
        const RecorderConfig& recorderConfig) {

    outputDir = recorderConfig.outputDirectory;
    PREVIEW_EVERY_N = recorderConfig.previewEveryNth;    
    showPreview = recorderConfig.preview;

    fps = cameraConfigs.begin()->second.fps;
    period = static_cast<int>(std::round(1000.0 / fps));

    DiscoverAndInit(cameraMapping);
    ConfigureAll(cameraConfigs);

    // Get the GigE transport layer.
    // We'll need it later to issue the action commands.
    CTlFactory& tlFactory = CTlFactory::GetInstance();
    pTL = dynamic_cast<IGigETransportLayer*>(tlFactory.CreateTl(BaslerGigEDeviceClass));

    // Use ouster if enabled
    if (recorderConfig.enableOuster)
    {
        ousterOutputDir = recorderConfig.outputDirectory;
        ouster = std::make_unique<OusterNode>(
            recorderConfig.ousterSensor
            );
    }
}

void CameraManager::DiscoverAndInit(const map<string, string>& cameraMapping) {
    CTlFactory& factory = CTlFactory::GetInstance();
    DeviceInfoList_t devices;

    if (factory.EnumerateDevices(devices) == 0)
        throw runtime_error("No cameras found");

    for (auto& [id, serial] : cameraMapping) {
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
    }
    std::cout << "Connected to " << cameras.size() << " cameras" << std::endl << std::endl;
}

void CameraManager::ConfigureAll(
    const std::map<std::string, CameraConfig>& cameraConfigs)
{
    for (auto& cam : cameras)
    {
        auto it = cameraConfigs.find(cam->logicalId);

        if (it == cameraConfigs.end())
        {
            std::cerr << "No configuration found for camera: "
                << cam->logicalId << std::endl;
            continue;
        }

        const CameraConfig& config = it->second;

        cam->Configure(config);

        //std::cout << "Configured camera: " << cam->logicalId << std::endl;
    }
    std::cout << std::endl;
}

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

            CCommandPtr(n.GetNode("GevIEEE1588DataSetLatch"))->Execute();
            auto status = CEnumerationPtr(n.GetNode("GevIEEE1588StatusLatched"))->ToString();

            std::cout << "Cam " << cam->logicalId << " PTP: " << status << std::endl;

            if (status == "Master") { master_count += 1; }

            if (status != "Master" && status != "Slave") {
                allSynced = false;
            }
        }
        if (master_count > 1) { allSynced = false; }

        this_thread::sleep_for(chrono::milliseconds(500));
    }

    // Wait for clocks to converge
    allSynced = false;
    int64_t maxOffset = 0;
    int i = 0;

    while (!allSynced) {
        maxOffset = 0;
        for (auto& cam : cameras) {
            INodeMap& n = cam->camera.GetNodeMap();

            CCommandPtr(n.GetNode("GevIEEE1588DataSetLatch"))->Execute();
            auto status = CEnumerationPtr(n.GetNode("GevIEEE1588StatusLatched"))->ToString();

            int64_t offsetFromMaster = CIntegerPtr(n.GetNode("GevIEEE1588OffsetFromMaster"))->GetValue();

            std::cout << "Cam " << cam->logicalId << " PTP: " << status << " Offset: " << offsetFromMaster << std::endl;

            std::max(maxOffset, std::abs(offsetFromMaster));
        }
        if (maxOffset < 5000) { i++; } // If less than 5 us / 5 000 ns
        else { i = 0; }
        if (i>5) { allSynced = true; } // If less than 5 us / 5 000 ns more than 5 times
        this_thread::sleep_for(chrono::milliseconds(500));
    }
    std::cout << "PTP synchronized across all cameras." << std::endl;
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

void CameraManager::SetupSynchronousFreeRun() {
    for (auto& cam : cameras)
    {
        cam->ConfigureSynchronousFreeRun();
    }

    Log("Synchronous Free Run configured.");
}

void CameraManager::Start(AcquisitionMode mode) {
    running = true;

    for (auto& cam : cameras)
        cam->camera.StartGrabbing(GrabStrategy_LatestImageOnly);

    // Consumer threads (Write and preview)
    consumerThread = thread(&CameraManager::ConsumeLoop, this);
    previewThread = thread(&CameraManager::PreviewLoop, this);

    // Threads retrieving images from the cameras and assigning them onto queue
    for (auto& cam : cameras)
        grabThreads.emplace_back(&CameraManager::GrabLoop, this, cam.get());

    // Create a thread to trigger if we are not using free run
    if (mode == AcquisitionMode::SoftwareTriggered) {
        triggerThread = thread(&CameraManager::TriggerLoop, this);
    }
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
    
    if (ouster) {
      ouster->Stop();
    }
}


// ============================ COMMANDS ============================


void CameraManager::FireActionCommand() {
    // Get the GigE transport layer.
    // We'll need it later to issue the action commands.
    //CTlFactory& tlFactory = CTlFactory::GetInstance();
    //IGigETransportLayer* pTL = dynamic_cast<IGigETransportLayer*>(tlFactory.CreateTl(BaslerGigEDeviceClass));

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

void CameraManager::FireScheduledActionCommand(uint64_t actionTime) {
    // Get the GigE transport layer.
    // We'll need it later to issue the action commands.
    //CTlFactory& tlFactory = CTlFactory::GetInstance();
    //IGigETransportLayer* pTL = dynamic_cast<IGigETransportLayer*>(tlFactory.CreateTl(BaslerGigEDeviceClass));

    //std::cout << "Trigger cameras!" << std::endl;

    // Issue action command to all interfaces
    pTL->IssueScheduledActionCommand(
        1,              // device key
        1,              // group key
        0xFFFFFFFF,     // group mask
        actionTime,
        "255.255.255.255"
    );

    //cout << "Scheduled action command fired." << endl;
}

void CameraManager::StartScheduledAcquisition() {
    auto& camera = cameras.front()->camera;
    INodeMap& nodeMap = camera.GetNodeMap();

    // Get current PTP timestamp
    CCommandPtr timestampLatch(nodeMap.GetNode("GevTimestampControlLatch"));
    timestampLatch->Execute();

    int64_t currentTimestamp = CIntegerPtr(nodeMap.GetNode("GevTimestampValue"))->GetValue();

    // Start 1 second from now
    constexpr int64_t START_DELAY_NS = 1'000'000'000;
    int64_t actionTime = currentTimestamp + START_DELAY_NS;

    FireScheduledActionCommand(actionTime);
}

bool CameraManager::IsRunning() const {
    return running.load();
}

void CameraManager::RequestSave() {
    std::cout << "Save requested!" << std::endl;
    saveTriggerId = triggerId.load() + 1;
}

void CameraManager::StartRecording() {
    std::cout << std::endl << "Starting recording..." << std::endl;
    
    std::string take_name;
    take_name = getTimeString();
    currentRecordingDir = outputDir + "/" + take_name + "/";
    createRecFolder(currentRecordingDir);

    // Start recording on Baslers
    for (auto& cam : cameras)
    {
        const std::string& id = cam->logicalId;
        frameWriters[id].Open(currentRecordingDir + id, cam->cameraConfiguration);
    }

    // Start Ouster recording
    if (ouster)
    {
        std::string lidarPath = ousterOutputDir + "/"  + take_name;

        createRecFolder(ousterOutputDir);

        if (!ouster->Start(lidarPath))
        {
            std::cerr << "[Ouster] Failed to start recording."
                << std::endl;
        }
    }

    recording = true;

    std::cout << "Recording started: " << currentRecordingDir << std::endl;
}

void CameraManager::StopRecording() {
    for (auto& [id, writer] : frameWriters)
    {
        writer.Close();
    }
    frameWriters.clear();

    // Stop Ouster
    if (ouster)
    {
        ouster->Stop();
    }

    recording = false;
    
    std::cout << "Recording stopped" << std::endl << std::endl;
}
void CameraManager::ToggleRecording() {
    if (recording) {
        StopRecording();
    }
    else {
        StartRecording();
    }
}

// ============================ THREADS =============================


void CameraManager::TriggerLoop() {
    while (running) {
        FireActionCommand();
        std::this_thread::sleep_for(std::chrono::milliseconds(period));
        triggerId++;
    }
    Log("Trigger loop exiting...");
}

void CameraManager::GrabLoop(CameraNode* cam) {
    CGrabResultPtr res;

    while (running && cam->camera.IsGrabbing()) {
    
        try {
            if (cam->camera.RetrieveResult(5000, res, TimeoutHandling_ThrowException)) {

                if (res->GrabSucceeded()) {
                    /*
                    Frame f{
                        cam->logicalId,
                        res->GetTimeStamp(),
                        res->GetBlockID(),
                        res
                    };
                    */                  
                    Frame f;

                    f.cameraId = cam->logicalId;
                    f.timestamp = res->GetTimeStamp();
                    f.frameId = res->GetBlockID();

                    const size_t imageSize = res->GetImageSize();

                    f.image.resize(imageSize);
                    std::memcpy(
                        f.image.data(),
                        res->GetBuffer(),
                        imageSize
                    );
                    
                    if (recording) {
                        frameQueue.push(std::move(f));
                    }

                    // Every Nth frame goes to preview
                    if (f.frameId % PREVIEW_EVERY_N == 0) {
                        previewQueue.push(std::move(f));
                    }
                    
                    //std::cout << cam->logicalId << " " << res->GetTimeStamp() << std::endl;
                    /*
                    if (recording) {
                        frameQueue.push(f);
                    }

                    // Every Nth frame goes to preview
                    if (f.frameId % PREVIEW_EVERY_N == 0) {
                        previewQueue.push(f);
                    }
                    */
                    
                }
                else
                {
                    std::cerr << "Grab failed. "
                        << "Camera: " << cam->logicalId
                        << "Error code: " << res->GetErrorCode()
                        << ", Description: " << res->GetErrorDescription()
                        << std::endl;
                }
            }
        }
        catch (const Pylon::TimeoutException& e) {
            std::cerr
                << "TIMEOUT: Camera " << cam->logicalId
                << ": " << e.what()
                << std::endl;
        }
        catch (const Pylon::GenericException& e) {
            std::cerr
                << "PYLON ERROR: Camera " << cam->logicalId
                << ": " << e.what()
                << std::endl;
            break;
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
        if (frameQueue.size() > 10)
        {
            Log("Queue size: " + std::to_string(frameQueue.size()));
        }
        if (recording) {
            //SaveRaw(f, currentRecordingDir);            
            frameWriters[f.cameraId].Write(f);
        }
        //else {
        //    SaveImage(f, currentRecordingDir);
        //}
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
        
        if (previewQueue.size() > 10)
        {
            Log("Preview Queue size: " + std::to_string(previewQueue.size()));
        }

        buffer[f.frameId][f.cameraId] = std::move(f);

        if (buffer[f.frameId].size() == numCameras) {

            auto& frames = buffer[f.frameId];

            const auto& firstFrame = frames.begin()->second;

            // Assuming all cameras have the same resolution
            int w = 1920;
            int h = 1200;

            // TODO: Store width/height in Frame if cameras can differ
            // int w = firstFrame.width;
            // int h = firstFrame.height;

            // TODO: Automatic grid size
            int cols = 2;
            int rows = 3;

            cv::Mat grid = cv::Mat::zeros(
                rows * h,
                cols * w,
                CV_8UC3
            );

            int i = 0;

            for (auto& [id, frame] : frames) {

                // Frame owns its own image data now.
                cv::Mat img(
                    h,
                    w,
                    CV_8UC1,
                    frame.image.data()
                );

                // Bayer to color
                cv::Mat imgColor;
                cv::cvtColor(
                    img,
                    imgColor,
                    cv::COLOR_BayerRG2RGB
                );

                // Camera ID
                cv::putText(
                    imgColor,
                    "Cam " + frame.cameraId,
                    cv::Point(30, 50),
                    cv::FONT_HERSHEY_SIMPLEX,
                    2.0,
                    cv::Scalar(0, 255, 0),
                    3
                );

                // Frame ID
                cv::putText(
                    imgColor,
                    "Frame " + std::to_string(frame.frameId),
                    cv::Point(35, 100),
                    cv::FONT_HERSHEY_SIMPLEX,
                    1.3,
                    cv::Scalar(255, 255, 0),
                    2
                );

                int r = i / cols;
                int c = i % cols;

                imgColor.copyTo(
                    grid(cv::Rect(
                        c * w,
                        r * h,
                        w,
                        h
                    ))
                );

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

            if (recording) {
                // Draw recording indicator
                cv::rectangle(
                    display,
                    cv::Point(0, 0),
                    cv::Point(display.cols - 1, display.rows - 1),
                    cv::Scalar(0, 0, 255),  // Red in BGR
                    5                         // Thickness
                );
            }

            cv::imshow("Preview   w: write 1 frame   r: toggle recording   ESC/q: exit", display);
            int key = cv::waitKey(1);

            if (key == 'q' || key == 27) { // ESC
                std::cout << "Exit requested..." << std::endl;
                running = false;
            }
            //else if (key == 'w') {
            //    RequestSave();              
            //}
            else if (key == 'r') {
                ToggleRecording();
            }
            /*
            else if (key == 't') {
                StopRecording();
            }
            */
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
