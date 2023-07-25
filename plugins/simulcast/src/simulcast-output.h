#pragma once

#include <obs.hpp>

#include <vector>

class SimulcastOutput {
public:
	void StartStreaming();
	void StopStreaming();
	bool IsStreaming();

private:
	OBSOutputAutoRelease SetupOBSOutput(obs_data_t *go_live_config);
	bool streaming_ = false;

	OBSOutputAutoRelease output_;
	std::vector<OBSEncoderAutoRelease> video_encoders_;
	OBSEncoderAutoRelease audio_encoder_;
};
