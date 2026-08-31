// Basler Multi-Camera Capture Template
// Requirements:
// - Pylon SDK 26.03
// - C++17
// - nlohmann/json

#include <pylon/PylonIncludes.h>

/*
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>
*/

#include <iostream>
#include <map>
#include <string>

#include "CameraNode.h"
#include "CameraManager.h"
#include "utils/file_io.h"
#include "configs/CameraConfig.h"
#include "configs/config_loader.h"

using namespace Pylon;
using namespace std;

//std::string out_folder = "./recordings/";

int main() {

    PylonInitialize();

    std::string recordingDir;

    try {

        // 0. Load recorder configuration
        RecorderConfig recorderConfig = LoadRecorderConfig("configs/recorder_config.json");
        recordingDir = recorderConfig.outputDirectory;
        /*
        // Create new recording folder
        std::string take_name;
        take_name = getTimeString();
        recorderConfig.outputDirectory = recorderConfig.outputDirectory + take_name + "/";
        createRecFolder(recorderConfig.outputDirectory);
        */
        CameraManager manager;

        // 1. Load which physical cameras are used and their logical names
        map<string, string> cameraMapping = LoadCameraMapping("configs/camera_mapping.json");

        // 2. Load default camera configuration
        CameraConfig defaultConfig = LoadCameraConfig("configs/camera_config.json");

        // 3. Build final configuration for every camera
        map<string, CameraConfig> cameraConfigs = BuildCameraConfigs(cameraMapping, defaultConfig);

        // 4. Find and initialize cameras using their individual configurations
        manager.Initialize(cameraMapping, cameraConfigs, recorderConfig);

        // 5. Wait for PTP synchronization
        manager.WaitForPtpSync();

        // 6. Setup trigger / SynchronousFreeRun
        // manager.SetupActionCommandTrigger();
        manager.SetupSynchronousFreeRun();

        // 7. Start grabbing
        manager.Start(AcquisitionMode::PtpScheduled);

        // Manager runs until OpenCV window receives stop command
        cout << "Press w to write 1 frame" << endl
            << "Press r to TOGGLE recording" << endl
            << "Press ESC or q to exit..." << endl;
        while (manager.IsRunning()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        manager.Stop();
    }
    catch (const exception& e) {
        cerr << "Error: " << e.what() << endl;

        // TODO: Remove the recording folder?
    }

    removeIfEmpty(recordingDir);

    PylonTerminate();

    cout << "Enter to exit..." << endl;
    std::getchar();
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