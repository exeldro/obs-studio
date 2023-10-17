#pragma once

#include <obs.hpp>
#include <string>

OBSDataArrayAutoRelease system_gpu_data();
OBSDataAutoRelease system_info();
std::string system_video_save_path();
