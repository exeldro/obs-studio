#include "goliveapi-postdata.hpp"

#include "immutable-date-time.hpp"
#include "system-info.hpp"
#include "multitrack-video-output.hpp"

OBSDataAutoRelease
constructGoLivePost(const ImmutableDateTime &attempt_start_time,
		    QString streamKey,
		    const std::optional<uint64_t> &maximum_aggregate_bitrate,
		    const std::optional<uint32_t> &reserved_encoder_sessions,
		    std::map<std::string, video_t *> &extra_views)
{
	OBSDataAutoRelease postData = obs_data_create();
	OBSDataAutoRelease capabilitiesData = obs_data_create();
	obs_data_set_string(postData, "service", "IVS");
	obs_data_set_string(postData, "schema_version", "2023-05-10");
	obs_data_set_string(postData, "stream_attempt_start_time",
			    attempt_start_time.CStr());
	obs_data_set_string(postData, "authentication",
			    streamKey.toUtf8().constData());
	obs_data_set_obj(postData, "capabilities", capabilitiesData);

	obs_data_set_bool(capabilitiesData, "plugin", true);

	obs_data_set_array(capabilitiesData, "gpu", system_gpu_data());

	auto systemData = system_info();
	obs_data_apply(capabilitiesData, systemData);

	obs_video_info ovi;
	if (obs_get_video_info(&ovi)) {
		OBSDataAutoRelease clientData = obs_data_create();
		obs_data_set_obj(capabilitiesData, "client", clientData);

		obs_data_set_string(clientData, "name", "obs-studio");
		obs_data_set_string(clientData, "version",
				    obs_get_version_string());
		obs_data_set_int(clientData, "width", ovi.output_width);
		obs_data_set_int(clientData, "height", ovi.output_height);
		obs_data_set_int(clientData, "fps_numerator", ovi.fps_num);
		obs_data_set_int(clientData, "fps_denominator", ovi.fps_den);

		obs_data_set_int(clientData, "canvas_width", ovi.base_width);
		obs_data_set_int(clientData, "canvas_height", ovi.base_height);
	}

	OBSDataArrayAutoRelease views = obs_data_array_create();

	for (auto &video_output : extra_views) {
		video_t *video = video_output.second;
		if (!video)
			continue;
		const struct video_output_info *voi =
			video_output_get_info(video);
		if (!voi)
			continue;
		OBSDataAutoRelease view = obs_data_create();
		obs_data_set_string(view, "name", video_output.first.c_str());
		obs_data_set_int(view, "width", voi->width);
		obs_data_set_int(view, "height", voi->height);
		obs_data_set_int(view, "fps_numerator", voi->fps_num);
		obs_data_set_int(view, "fps_denominator", voi->fps_den);
		obs_data_array_push_back(views, view);
	}
	obs_data_set_array(capabilitiesData, "extra_views", views);

	OBSDataAutoRelease preferences = obs_data_create();
	obs_data_set_obj(postData, "preferences", preferences);
	if (maximum_aggregate_bitrate.has_value())
		obs_data_set_int(preferences, "maximum_aggregate_bitrate",
				 maximum_aggregate_bitrate.value());
	if (reserved_encoder_sessions.has_value())
		obs_data_set_int(preferences, "reserved_encoder_sessions",
				 reserved_encoder_sessions.value());

#if 0
	// XXX hardcoding the present-day AdvancedOutput behavior here..
	// XXX include rescaled output size?
	OBSData encodingData = AdvancedOutputStreamEncoderSettings();
	obs_data_set_string(encodingData, "type",
			    config_get_string(config, "AdvOut", "Encoder"));
	unsigned int cx, cy;
	AdvancedOutputGetRescaleRes(config, &cx, &cy);
	if (cx && cy) {
		obs_data_set_int(encodingData, "width", cx);
		obs_data_set_int(encodingData, "height", cy);
	}

	OBSDataArray encodingDataArray = obs_data_array_create();
	obs_data_array_push_back(encodingDataArray, encodingData);
	obs_data_set_array(postData, "client_encoder_configurations",
			   encodingDataArray);
#endif

	//XXX todo network.speed_limit

	return postData;
}
