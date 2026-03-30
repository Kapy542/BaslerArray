#include "fileio.h"

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