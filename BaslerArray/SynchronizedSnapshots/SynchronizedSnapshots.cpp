// Basler Multi-Camera Capture Template
// Requirements:
// - Pylon SDK 26.03
// - C++17
// - nlohmann/json

#include <pylon/PylonIncludes.h>
#include <pylon/BaslerUniversalInstantCamera.h>
#include <pylon/gige/GigETransportLayer.h>

#include <iostream>
#include <vector>
#include <thread>
#include <atomic>
#include <map>
#include <fstream>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <filesystem>
#include <sstream>
#include <iomanip>

#include <nlohmann/json.hpp>

#include "fileio.h"

using namespace Pylon;
using namespace GenApi;
using namespace std;
using json = nlohmann::json;

std::string out_folder = "./recordings/";

// ========================= CONFIG =========================
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

CameraConfig LoadConfig(const string& filename) {
    ifstream file(filename);
    if (!file.is_open()) throw runtime_error("Failed to open config file");

    json j; file >> j;

    CameraConfig cfg;
    cfg.width = j.value("width", 0);
    cfg.height = j.value("height", 0);
    cfg.exposure = j.value("exposure", 0.0);
    cfg.gain = j.value("gain", 0.0);

    cfg.lightSourceSelector = j.at("lightSourceSelector").get<std::string>();

    for (const auto& item : j.at("balanceRatioSelector")) {
        CameraConfig::BalanceRatio br;
        br.selector = item.at("selector").get<std::string>();
        br.balanceRatioRaw = item.at("balanceRatioRaw").get<int>();
        cfg.balanceRatios.push_back(br);
    }

    return cfg;
}

// ========================= FRAME =========================
struct Frame {
    string cameraId;
    uint64_t timestamp;
    uint64_t frameId;
    CGrabResultPtr grab;
};

// ========================= SAFE QUEUE =========================
template<typename T>
class SafeQueue {
    queue<T> q;
    mutex m;
    condition_variable cv;
    bool stopped = false;

public:
    void push(const T& item) {
        {
            lock_guard<mutex> lock(m);
            q.push(item);
        }
        cv.notify_one();
    }

    // Queue needs to be stopped in order to prevent consumer getting stuck when exiting
    void stop() {
        {
            lock_guard<mutex> lock(m);
            stopped = true;
        }
        cv.notify_all();
    }

    bool pop(T& item) {
        unique_lock<mutex> lock(m);

        cv.wait(lock, [&] {
            return !q.empty() || stopped;
        });

        if (q.empty()) {
            return false; // stopped and no data
        }

        item = q.front();
        q.pop();
        return true;
    }
};


// ========================= IMAGE SAVING =========================
namespace fs = std::filesystem;

// Save using Pylon (cross-platform, no extra deps)
void SaveImage(const Frame& f, const std::string& baseDir) {
    try {
        // Create per-camera directory: output/01/, output/02/, ...
        fs::path dir = fs::path(baseDir) / f.cameraId;
        if (!fs::exists(dir)) {
            fs::create_directories(dir);
        }

        // Build filename: camId_timestamp_frameId.png
        std::ostringstream name;
        name << f.cameraId << "_" << f.timestamp << "_" << f.frameId << ".png";
        fs::path filepath = dir / name.str();

        // Save via Pylon ImagePersistence
        CImagePersistence::Save(
            ImageFileFormat_Png,
            filepath.string().c_str(),
            f.grab
        );
    }
    catch (const GenericException& e) {
        cerr << "Save error: " << e.GetDescription() << endl;
    }
}

// ========================= LOGGER =========================
void Log(const string& msg) {
    cout << "[" << get_time_string() << "] " << msg << std::endl;
}

// ========================= CAMERA NODE =========================
class CameraNode {
public:
    CBaslerUniversalInstantCamera camera;
    string serial;
    string logicalId;

    CameraNode(IPylonDevice* device, const string& id)
        : camera(device), logicalId(id) {
        serial = camera.GetDeviceInfo().GetSerialNumber();
    }

    void Configure(const CameraConfig& cfg) {
        // TODO: Format, PTP?, White Balance, FPS?, Jumbo frames and delay?
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
        }
        catch (const GenericException& e) {
            cerr << "Config error: " << e.GetDescription() << endl;
        }
    }

    void ConfigureActionTrigger(uint32_t deviceKey,
        uint32_t groupKey,
        uint32_t groupMask) {
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

    void EnablePTP() {
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
};

// ========================= CAMERA MANAGER =========================
class CameraManager {
private:
    vector<unique_ptr<CameraNode>> cameras;
    SafeQueue<Frame> frameQueue;
    atomic<bool> running{ false };
    vector<thread> grabThreads;
    thread consumerThread;
    std::string outputDir;

public:
    CameraManager(const std::string& dir) : outputDir(dir) {}

    map<string, string> LoadCameraOrder(const string& filename) {
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

    void DiscoverAndInit(const map<string, string>& order) {
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
            cam->EnablePTP();
        }
    }

    void ConfigureAll(const CameraConfig& cfg) {
        for (auto& cam : cameras)
            cam->Configure(cfg);
    }

    void SetupActionCommandTrigger() {
        const uint32_t deviceKey = 1;
        const uint32_t groupKey = 1;
        const uint32_t groupMask = 0xFFFFFFFF;

        for (auto& cam : cameras) {
            cam->ConfigureActionTrigger(deviceKey, groupKey, groupMask);
        }

        cout << "Action command trigger configured." << endl;
    }

    void WaitForPtpSync() {
        cout << "Waiting for PTP synchronization..." << endl;

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

    void Start() {
        running = true;

        for (auto& cam : cameras)
            cam->camera.StartGrabbing(GrabStrategy_LatestImageOnly);

        // consumer thread
        consumerThread = thread(&CameraManager::ConsumeLoop, this);

        // grab threads
        for (auto& cam : cameras)
            grabThreads.emplace_back(&CameraManager::GrabLoop, this, cam.get());
    }

    void Stop() {
        frameQueue.stop(); // Stop the queue so consumer won't get stuck
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
    }

    void Trigger() {
        std::cout << "Trigger cameras!" << std::endl;
    }

    void FireActionCommand() {
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

private:
    void GrabLoop(CameraNode* cam) {
        CGrabResultPtr res;
        while (running && cam->camera.IsGrabbing()) {
            if (cam->camera.RetrieveResult(50000, res, TimeoutHandling_ThrowException)) {
                if (res->GrabSucceeded()) {

                    Log( "Got a image from: " + cam->logicalId );
                    
                    frameQueue.push({
                        cam->logicalId,
                        res->GetTimeStamp(),
                        res->GetBlockID(),
                        res
                        });
                    
                }
            }
        }
    }

    void ConsumeLoop() {
        while (running) {
            Frame f;

            if (!frameQueue.pop(f)) {
                break; // queue stopped
            }

            Log("Writing " + f.cameraId + " Frame " + to_string(f.frameId) +
                " Timestamp " + to_string(f.timestamp) + "\n");

            SaveImage(f, outputDir);
        }

        cout << "Consumer thread exiting." << endl;
    }

    // TODO: ?
    void displayLoop() {
        while (running) {
            // TODO: 
        }
    }
};

// ========================= MAIN =========================
int main() {
    PylonInitialize();

    try {
        // Create new recording folder
        std::string take_name;
        take_name = get_time_string();
        out_folder = out_folder + take_name + "/";
        create_rec_folder(out_folder);

        CameraManager manager(out_folder);

        auto cfg = LoadConfig("camera_config.json");
        auto order = manager.LoadCameraOrder("camera_order.json");

        manager.DiscoverAndInit(order);
        manager.ConfigureAll(cfg);

        // 1. Wait for PTP sync to decide master/slave relationship TODO: and offset to settle
        //manager.WaitForPtpSync();

        // 2. Setup trigger
        manager.SetupActionCommandTrigger();

        // x. Start grabbing
        manager.Start();

        cout << "Enter w to write image" << endl 
             << "Enter q to stop..." << endl;
        while (true) {
            std::string input;
            std::getline(std::cin, input);

            if (input == "q") {
                cout << "Exiting..." << endl;
                break;
            }
            else if (input == "w") {
                manager.FireActionCommand();
            }
        }       

        manager.Stop();
    }
    catch (const exception& e) {
        cerr << "Error: " << e.what() << endl;

        // TODO: Remove the recording folder
    }

    // TODO: If the recording folder is empty -> Remove the recording folder

    PylonTerminate();
    return 0;
}

/* SAMPLE camera_config.json
{
  "width": 1920,
  "height": 1080,
  "exposure": 5000.0,
  "gain": 5.0
}

SAMPLE camera_order.json
{
  "01": "12345678",
  "02": "87654321"
}
*/

