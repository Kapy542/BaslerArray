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

std::string out_folder = "./recordings/";

int main() {
    /*
    int width = 640, height = 480;
    cv::Mat src(height, width, CV_8UC3, cv::Scalar(255, 255, 255));
    cv::imshow("Image", src);
    cv::waitKey(0);
    */
    PylonInitialize();

    try {
        // Create new recording folder
        std::string take_name;
        take_name = get_time_string();
        out_folder = out_folder + take_name + "/";
        create_rec_folder(out_folder);

        CameraManager manager(out_folder);

        CameraConfig cfg = LoadConfig("configs/camera_config.json");
        map<string, string> order = manager.LoadCameraOrder("configs/camera_order.json");

        manager.Initialize(cfg, order);
        //manager.DiscoverAndInit(order);
        //manager.ConfigureAll(cfg);

        // 1. Wait for PTP sync to decide master/slave relationship
        manager.WaitForPtpSync();

        // 2. Setup trigger / SynchronousFreeRun
        // manager.SetupActionCommandTrigger();
        manager.SetupSynchronousFreeRun(2.0);

        // x. Start grabbing
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

    remove_if_empty(out_folder);

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