#include "Log.h"

#include <iostream>
#include <mutex>

std::mutex logMutex;

void Log(const std::string& msg) {
    std::lock_guard<std::mutex> lock(logMutex);

    std::cout << msg << std::endl;
}