#include "simulcast-output.h"

#include <util/dstr.hpp>
#include <util/platform.h>
#include <util/util.hpp>
#include <obs-frontend-api.h>
#include <obs-module.h>

#include "plugin-macros.generated.h"

static OBSOutputAutoRelease create_output()
{
	OBSOutputAutoRelease output = obs_output_create(
		"rtmp_output_simulcast", "rtmp simulcast", nullptr, nullptr);
	if (!output) {
		blog(LOG_ERROR, "failed to create simulcast rtmp output");
		return nullptr;
	}

	obs_output_set_service(output, obs_frontend_get_streaming_service());
	return output;
}

static OBSEncoderAutoRelease create_video_encoder(DStr &name_buffer,
						  size_t encoder_index)
{
	dstr_printf(name_buffer, "simulcast video encoder %zu", encoder_index);
	OBSEncoderAutoRelease video_encoder = obs_video_encoder_create(
		"jim_nvenc", name_buffer, nullptr, nullptr);
	if (!video_encoder) {
		blog(LOG_ERROR, "failed to create video encoder '%s'",
		     name_buffer->array);
		return nullptr;
	}
	obs_encoder_set_video(video_encoder, obs_get_video());
	return video_encoder;
}

static OBSEncoderAutoRelease create_audio_encoder()
{
	OBSEncoderAutoRelease audio_encoder = obs_audio_encoder_create(
		"ffmpeg_aac", "simulcast aac", nullptr, 0, nullptr);
	if (!audio_encoder) {
		blog(LOG_ERROR, "failed to create audio encoder");
		return nullptr;
	}
	obs_encoder_set_audio(audio_encoder, obs_get_audio());
	return audio_encoder;
}

static OBSDataAutoRelease load_simulcast_config()
{
	BPtr simulcast_path = obs_module_file("simulcast.json");
	if (!simulcast_path) {
		blog(LOG_ERROR, "Failed to get simulcast.json path");
		return nullptr;
	}

	OBSDataAutoRelease data =
		obs_data_create_from_json_file(simulcast_path);
	if (!data)
		blog(LOG_ERROR, "Failed to read simulcast.json");

	return data;
}

void SimulcastOutput::StartStreaming()
{
	auto config = load_simulcast_config();
	if (!config)
		return;

	output_ = SetupOBSOutput(config);

	if (!output_)
		return;

	if (!obs_output_start(output_)) {
		blog(LOG_WARNING, "Failed to start stream");
		return;
	}

	blog(LOG_INFO, "starting stream");

	streaming_ = true;
}

void SimulcastOutput::StopStreaming()
{
	if (output_)
		obs_output_stop(output_);

	output_ = nullptr;
	video_encoders_.clear();
	audio_encoder_ = nullptr;

	streaming_ = false;
}

bool SimulcastOutput::IsStreaming()
{
	return streaming_;
}

OBSOutputAutoRelease SimulcastOutput::SetupOBSOutput(obs_data_t *go_live_config)
{

	auto output = create_output();

	OBSDataArrayAutoRelease encoder_configs =
		obs_data_get_array(go_live_config, "encoder_configurations");
	DStr video_encoder_name_buffer;
	for (size_t i = 0; i < obs_data_array_count(encoder_configs); i++) {
		auto encoder =
			create_video_encoder(video_encoder_name_buffer, i);
		obs_output_set_video_encoder2(output, encoder, i);
		video_encoders_.emplace_back(std::move(encoder));
	}

	audio_encoder_ = create_audio_encoder();
	obs_output_set_audio_encoder(output, audio_encoder_, 0);

	return output;
}
