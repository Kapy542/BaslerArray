#pragma once

#include <string>

#include <cstdint>
#include <cstddef>
#include <fstream>

#include "../core/Frame.h"
#include "../configs/CameraConfig.h"

std::string getTimeString();

bool createRecFolder(std::string path);

void removeRecFolder(std::string path);

void removeIfEmpty(std::string path);

void SaveImage(const Frame& f, const std::string& baseDir);

void SaveRaw(const Frame& f, const std::string& baseDir);

void SaveMetadata(
    const std::string& directory,
    const CameraConfig& config);


class FrameWriter
{
public:
    FrameWriter() = default;
    ~FrameWriter();

    void Open(const std::string& directory, const CameraConfig& config);
    void Write(const Frame& f);
    void Close();

    bool IsOpen() const;

private:
    std::ofstream frameFile;
    std::ofstream timestampFile;
};