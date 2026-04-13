#pragma once
#include <map>
#include <string>
#include <thread>
#include <atomic>

#include "CameraNode.h"
#include "core/Frame.h"
#include "core/SafeQueue.h"
#include "configs/CameraConfig.h"

class CameraManager 
{
private:
    vector<unique_ptr<CameraNode>> cameras;
    atomic<bool> running{ false };
    std::atomic<uint64_t> triggerId = 0;
    std::atomic<uint64_t> saveTriggerId = -1;
    SafeQueue<Frame> frameQueue;
    SafeQueue<Frame> previewQueue;   
    vector<thread> grabThreads;
    thread consumerThread;
    thread previewThread;
    std::string outputDir;

public:
    CameraManager(const std::string& dir);
    //~CameraManager();

    std::map<std::string, std::string> LoadCameraOrder(const std::string& filename);

    void DiscoverAndInit(const  std::map<std::string, std::string>& order);

    void ConfigureAll(const CameraConfig& cfg);

    void SetupActionCommandTrigger();

    void WaitForPtpSync();

    void Start();

    void Stop();

    void FireActionCommand();

    bool IsRunning();

private:
    void TriggerLoop();

    void GrabLoop(CameraNode* cam);

    void ConsumeLoop();

    void PreviewLoop();

    void RequestSave();
};