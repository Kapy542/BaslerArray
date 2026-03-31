// Basler Multi-Camera Capture Template
// Requirements:
// - Pylon SDK 26.03
// - C++17
// - nlohmann/json

#include <pylon/PylonIncludes.h>

#include <iostream>
#include <map>
#include <string>

#include "CameraNode.h"
#include "CameraManager.h"
#include "utils/file_io.h"
#include "configs/CameraConfig.h"
#include "configs/config_loader.h"


using namespace Pylon;
//using namespace GenApi;
using namespace std;
//using json = nlohmann::json;

std::string out_folder = "./recordings/";


int main() {
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

