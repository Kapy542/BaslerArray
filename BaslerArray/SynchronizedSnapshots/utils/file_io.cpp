#include "file_io.h"

#include <pylon/PylonIncludes.h>

#include <iostream>
#include <ctime>
#include <chrono>
#include <filesystem>

#include <ctime>
#include <iomanip>
#include <sstream>

using namespace Pylon;

std::string get_time_string()
{
    std::time_t rawtime = std::time(nullptr);
    std::tm now;
    //localtime_s(&now, &rawtime);
    #ifdef _WIN32
        localtime_s(&now, &rawtime);
    #else
        localtime_r(&rawtime, &now);
    #endif

    std::ostringstream oss;
    oss << std::put_time(&now, "%Y-%m-%d--%H-%M-%S");
    return oss.str();
}

bool create_rec_folder(std::string path)
{
    bool success;
    std::filesystem::create_directories(path);
    success = std::filesystem::exists(path);
    if (success) { std::cout << "Created recoding directory: " << path << std::endl; }
    else { std::cout << "Could NOT create recoding directory: " << path << std::endl; }
    return success;
}

void remove_rec_folder(std::string path)
{
    std::filesystem::remove_all(path);
}

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
        std::cerr << "Save error: " << e.GetDescription() << std::endl;
    }
}