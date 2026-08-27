#include "file_io.h"

#include <pylon/PylonIncludes.h>

#include <iostream>
#include <ctime>
#include <chrono>
#include <filesystem>

#include <ctime>
#include <iomanip>
#include <fstream>
#include <sstream>

using namespace Pylon;

std::string getTimeString()
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

bool createRecFolder(std::string path)
{
    bool success;
    std::filesystem::create_directories(path);
    success = std::filesystem::exists(path);
    if (success) { std::cout << "Created recoding directory: " << path << std::endl; }
    else { std::cout << "Could NOT create recoding directory: " << path << std::endl; }
    return success;
}

void removeRecFolder(std::string path)
{
    std::filesystem::remove_all(path);
}

void removeIfEmpty(std::string path)
{
    if (std::filesystem::is_empty(path)) {
        std::filesystem::remove_all(path);
    }
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
        name << f.cameraId << "_" 
             << f.timestamp << "_" 
             << f.frameId << ".png";

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

// Save raw image
void SaveRaw(const Frame& f, const std::string& baseDir)
{
    try
    {
        // Create per-camera directory: output/01/, output/02/, ...
        fs::path dir = fs::path(baseDir) / f.cameraId;
        fs::create_directories(dir);

        // Build filename: camId_timestamp_frameId.raw
        std::ostringstream name;
        name << f.cameraId << "_"
             << f.timestamp << "_"
             << f.frameId << ".raw";

        fs::path filepath = dir / name.str();

        // Get image data from Pylon grab result
        const void* buffer = f.grab->GetBuffer();
        size_t size = f.grab->GetImageSize();

        // Write binary file
        std::ofstream file(filepath, std::ios::binary);

        if (!file.is_open())
        {
            std::cerr << "Failed to open file: " << filepath << std::endl;
            return;
        }

        file.write(reinterpret_cast<const char*>(buffer),
            static_cast<std::streamsize>(size));

    }
    catch (const Pylon::GenericException& e)
    {
        std::cerr << "SaveRaw Pylon error: " << e.GetDescription() << std::endl;
    }
    catch (const std::exception& e)
    {
        std::cerr << "SaveRaw error: " << e.what() << std::endl;
    }
}