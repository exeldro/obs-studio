#include "ivs-events.hpp"
#include "goliveapi-censoredjson.hpp"

#include "immutable-date-time.hpp"

#include <util/dstr.hpp>

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
	return OBSDataAutoRelease{obs_data_create()};
}

OBSDataAutoRelease MakeEvent_ivs_obs_stream_stopped(long long *error_code,
						    const char *error_string)
{
	DStr client_error;
	if (error_code && !error_string)
		dstr_printf(client_error, "%lld", *error_code);
	else if (error_code && error_string)
		dstr_printf(client_error, "%lld: %s", *error_code,
			    error_string);

	OBSDataAutoRelease event = obs_data_create();
	obs_data_set_string(event, "client_error",
			    client_error->array ? client_error->array : "");
	obs_data_set_string(event, "server_error", "");

	if (error_code)
		obs_data_set_int(event, "error_code", *error_code);
	if (error_string)
		obs_data_set_string(event, "error_string", error_string);

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
