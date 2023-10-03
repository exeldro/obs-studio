#include "system-info.h"

OBSDataArrayAutoRelease system_gpu_data()
{
	return nullptr;
}

OBSDataAutoRelease system_info()
{
	return nullptr;
}

std::string system_video_save_path()
{
	return std::string(getenv("HOME"));
}
