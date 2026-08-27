#pragma once

#include <string>

#include "../core/Frame.h"

std::string getTimeString();

bool createRecFolder(std::string path);

void removeRecFolder(std::string path);

void removeIfEmpty(std::string path);

void SaveImage(const Frame& f, const std::string& baseDir);

void SaveRaw(const Frame& f, const std::string& baseDir);
