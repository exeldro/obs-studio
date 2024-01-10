#pragma once

#include <obs.hpp>

#include <optional>

#include "immutable-date-time.hpp"

OBSDataAutoRelease MakeEvent_ivs_obs_stream_start(
	obs_data_t *postData, obs_data_t *goLiveConfig,
	const ImmutableDateTime &stream_attempt_start_time,
	qint64 msecs_elapsed_after_go_live_config_download,
	qint64 msecs_elapsed_after_start_streaming_returned,
	std::optional<int> connect_time_ms);

OBSDataAutoRelease MakeEvent_ivs_obs_stream_start_failed(
	obs_data_t *postData, obs_data_t *goLiveConfig,
	const ImmutableDateTime &stream_attempt_start_time,
	qint64 msecs_elapsed_after_go_live_config_download,
	qint64 msecs_elapsed_after_start_streaming_returned);

OBSDataAutoRelease MakeEvent_ivs_obs_stream_stop();

OBSDataAutoRelease MakeEvent_ivs_obs_stream_stopped(long long *error_code,
						    const char *error_string);

OBSDataAutoRelease
MakeEvent_ivs_obs_stream_started(qint64 msecs_elapsed_after_started_signal);
