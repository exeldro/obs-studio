#include "ivs-events.h"

OBSDataAutoRelease MakeEvent_ivs_obs_stream_start(obs_data_t *postData,
						  obs_data_t *goLiveConfig)
{
	OBSDataAutoRelease event = obs_data_create();

	// include the entire capabilities API request/response
	obs_data_set_string(event, "capabilities_api_request",
			    obs_data_get_json(postData));

	obs_data_set_string(event, "capabilities_api_response",
			    obs_data_get_json(goLiveConfig));

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

OBSDataAutoRelease MakeEvent_ivs_obs_stream_stop()
{
	OBSDataAutoRelease event = obs_data_create();
	obs_data_set_string(event, "client_error", "");
	obs_data_set_string(event, "server_error", "");
	return event;
}
