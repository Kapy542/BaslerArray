#pragma once

#include <string>

#include "../core/Frame.h"

std::string get_time_string();

bool create_rec_folder(std::string path);

void remove_rec_folder(std::string path);

void SaveImage(const Frame& f, const std::string& baseDir);
