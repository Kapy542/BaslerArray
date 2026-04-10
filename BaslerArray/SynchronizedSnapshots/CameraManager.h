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
    SafeQueue<Frame> frameQueue;
    SafeQueue<Frame> previewQueue;
    atomic<bool> running{ false };
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

private:
    void GrabLoop(CameraNode* cam);

    void ConsumeLoop();

    // TODO: ?
    void PreviewLoop();
};