#pragma once

#include <obs.hpp>

#include "immutable-date-time.h"

OBSDataAutoRelease MakeEvent_ivs_obs_stream_start(
	obs_data_t *postData, obs_data_t *goLiveConfig,
	const ImmutableDateTime &stream_attempt_start_time,
	qint64 msecs_elapsed_after_go_live_config_download,
	qint64 msecs_elapsed_after_start_streaming_returned);

OBSDataAutoRelease MakeEvent_ivs_obs_stream_start_failed(
	obs_data_t *postData, obs_data_t *goLiveConfig,
	const ImmutableDateTime &stream_attempt_start_time,
	qint64 msecs_elapsed_after_go_live_config_download,
	qint64 msecs_elapsed_after_start_streaming_returned);

OBSDataAutoRelease MakeEvent_ivs_obs_stream_stop();
