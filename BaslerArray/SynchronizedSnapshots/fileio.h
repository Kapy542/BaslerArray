#pragma once

#include <string>
#include <iostream>
#include <ctime>
#include <chrono>
#include <filesystem>

#include <ctime>
#include <iomanip>
#include <sstream>

std::string get_time_string();

bool create_rec_folder(std::string path);

void remove_rec_folder(std::string path);