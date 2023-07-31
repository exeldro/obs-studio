#pragma once

#include <obs.hpp>

OBSDataAutoRelease MakeEvent_ivs_obs_stream_start(obs_data_t *postData,
						  obs_data_t *goLiveConfig);
OBSDataAutoRelease MakeEvent_ivs_obs_stream_stop();
