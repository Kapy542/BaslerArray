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

#include <nlohmann/json.hpp>

using json = nlohmann::json;
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


void SaveMetadata(
    const std::string& directory,
    const CameraConfig& config)
{
    json metadata;

    metadata["width"] = config.width;
    metadata["height"] = config.height;
   // metadata["pixelFormat"] = config.pixelFormat;

    metadata["fps"] = config.fps;

    metadata["reverseX"] = config.reverseX;
    metadata["reverseY"] = config.reverseY;

    metadata["exposureUs"] = config.exposureUs;
    metadata["gain"] = config.gainRaw;

    metadata["exposureAuto"] = config.exposureAuto;
    metadata["gainAuto"] = config.gainAuto;

    metadata["whiteBalance"]["mode"] = config.whiteBalance.mode;

    metadata["whiteBalance"]["lightSource"] = config.whiteBalance.lightSource;

    metadata["whiteBalance"]["red"] = config.whiteBalance.red;

    metadata["whiteBalance"]["green"] = config.whiteBalance.green;

    metadata["whiteBalance"]["blue"] = config.whiteBalance.blue;

    metadata["packetSize"] = config.packetSize;

    // Document how the binary files are stored.
    metadata["timestampFormat"] = "uint64";
    metadata["timestampUnit"] = "ns";

    std::ofstream file(
        std::filesystem::path(directory) / "metadata.json");

    if (!file)
    {
        throw std::runtime_error(
            "Failed to create metadata.json in: " + directory);
    }

    file << metadata.dump(4);
}


////////////////
// FameWriter //
////////////////

void FrameWriter::Open(const std::string& directory, const CameraConfig& config)
{
    // Make sure the directory exists
    std::filesystem::create_directories(directory);

    const std::filesystem::path framePath =
        std::filesystem::path(directory) / "frames.bin";

    const std::filesystem::path timestampPath =
        std::filesystem::path(directory) / "timestamps.bin";

    frameFile.open(framePath, std::ios::binary);
    timestampFile.open(timestampPath, std::ios::binary);

    if (!frameFile.is_open())
    {
        throw std::runtime_error(
            "Failed to open frame file: " + framePath.string());
    }

    if (!timestampFile.is_open())
    {
        frameFile.close();

        throw std::runtime_error(
            "Failed to open timestamp file: " + timestampPath.string());
    }

    SaveMetadata(directory, config);

    std::cout << "Opened recording files in: "
        << directory << std::endl;
}

void FrameWriter::Write(const Frame& f)
{
    // Get image data from Pylon grab result
    const void* buffer = f.grab->GetBuffer();
    size_t size = f.grab->GetImageSize();

    frameFile.write(
        reinterpret_cast<const char*>(buffer),
        static_cast<std::streamsize>(size));

    timestampFile.write(
        reinterpret_cast<const char*>(&f.timestamp),
        sizeof(f.timestamp));
}

void FrameWriter::Close()
{
    if (frameFile.is_open())
        frameFile.close();

    if (timestampFile.is_open())
        timestampFile.close();
}

bool FrameWriter::IsOpen() const
{
    return frameFile.is_open() &&
        timestampFile.is_open();
}

FrameWriter::~FrameWriter()
{
    Close();
}