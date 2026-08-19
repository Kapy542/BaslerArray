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
    std::atomic<bool> running{ false };

    std::atomic<uint64_t> triggerId = 1;      // Current trigger id: Updated by trigger loop on every trigger
    std::atomic<uint64_t> saveTriggerId = -1; // ID to be written on disk: Set by preview loop to save upcoming image (triggerId + N)

    std::atomic<bool> recording{ false };     // Flag for continuous recording

    SafeQueue<Frame> frameQueue;
    SafeQueue<Frame> previewQueue;   
    vector<thread> grabThreads;
    thread consumerThread;
    thread previewThread;
    thread triggerThread;
    std::string outputDir;

public:
    CameraManager(const std::string& dir);
    ~CameraManager();

    std::map<std::string, std::string> LoadCameraOrder(const std::string& filename);

    void DiscoverAndInit(const  std::map<std::string, std::string>& order);

    void ConfigureAll(const CameraConfig& cfg);

    void SetupActionCommandTrigger();

    void WaitForPtpSync();

    void Start();

    void Stop();

    void FireActionCommand();

    bool IsRunning() const;

private:
    void TriggerLoop();

    void GrabLoop(CameraNode* cam);

    void ConsumeLoop();

    void PreviewLoop();

    void RequestSave();

    void StartRecording();

    void StopRecording();
};