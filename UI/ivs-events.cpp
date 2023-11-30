#include "ivs-events.hpp"
#include "goliveapi-censoredjson.hpp"

#include "immutable-date-time.hpp"

OBSDataAutoRelease MakeEvent_ivs_obs_stream_start(
	obs_data_t *postData, obs_data_t *goLiveConfig,
	const ImmutableDateTime &stream_attempt_start_time,
	qint64 msecs_elapsed_after_go_live_config_download,
	qint64 msecs_elapsed_after_start_streaming_returned,
	std::optional<int> connect_time_ms)
{
	OBSDataAutoRelease event = obs_data_create();

	// include the entire capabilities API request/response
	obs_data_set_string(event, "capabilities_api_request",
			    censoredJson(postData).toUtf8().constData());

	obs_data_set_string(event, "capabilities_api_response",
			    censoredJson(goLiveConfig).toUtf8().constData());

	obs_data_set_string(event, "stream_attempt_start_time",
			    stream_attempt_start_time.CStr());
	obs_data_set_int(event, "msecs_elapsed_until_go_live_config_download",
			 msecs_elapsed_after_go_live_config_download);
	obs_data_set_int(event, "msecs_elapsed_until_start_streaming_returned",
			 msecs_elapsed_after_start_streaming_returned);

	if (connect_time_ms.has_value())
		obs_data_set_int(event, "connect_time_msecs", *connect_time_ms);

	// extract specific items of interest from the capabilities API response
	OBSDataAutoRelease goLiveMeta = obs_data_get_obj(goLiveConfig, "meta");
	if (goLiveMeta) {
		const char *s = obs_data_get_string(goLiveMeta, "config_id");
		if (s && *s) {
			obs_data_set_string(event, "config_id", s);
		}
	}

	OBSDataArrayAutoRelease goLiveEncoderConfigurations =
		obs_data_get_array(goLiveConfig, "encoder_configurations");
	if (goLiveEncoderConfigurations) {
		obs_data_set_int(
			event, "encoder_count",
			obs_data_array_count(goLiveEncoderConfigurations));
	}

	return event;
}

OBSDataAutoRelease MakeEvent_ivs_obs_stream_start_failed(
	obs_data_t *postData, obs_data_t *goLiveConfig,
	const ImmutableDateTime &stream_attempt_start_time,
	qint64 msecs_elapsed_after_go_live_config_download,
	qint64 msecs_elapsed_after_start_streaming_returned)
{
	return MakeEvent_ivs_obs_stream_start(
		postData, goLiveConfig, stream_attempt_start_time,
		msecs_elapsed_after_go_live_config_download,
		msecs_elapsed_after_start_streaming_returned, std::nullopt);
}

OBSDataAutoRelease MakeEvent_ivs_obs_stream_stop()
{
	OBSDataAutoRelease event = obs_data_create();
	obs_data_set_string(event, "client_error", "");
	obs_data_set_string(event, "server_error", "");
	return event;
}

OBSDataAutoRelease
MakeEvent_ivs_obs_stream_started(qint64 msecs_elapsed_after_started_signal)
{
	OBSDataAutoRelease event = obs_data_create();
	obs_data_set_int(event, "msecs_elapsed_until_stream_started",
			 msecs_elapsed_after_started_signal);
	return event;
}
