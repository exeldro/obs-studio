#include "simulcast-output.h"

#include <util/dstr.hpp>
#include <util/platform.h>
#include <util/util.hpp>
#include <obs-frontend-api.h>
#include <obs-module.h>

#include <cinttypes>
#include <cmath>
#include <numeric>

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

struct pixel_resolution {
	uint32_t width;
	uint32_t height;
};

static pixel_resolution scale_resolution(const obs_video_info &ovi,
					 uint64_t requested_width,
					 uint64_t requested_height)
{
	auto aspect_segments = std::gcd(ovi.base_width, ovi.base_height);
	auto aspect_width = ovi.base_width / aspect_segments;
	auto aspect_height = ovi.base_height / aspect_segments;

	auto base_pixels =
		static_cast<uint64_t>(ovi.output_width) * ovi.output_height;

	auto requested_pixels = requested_width * requested_height;

	auto pixel_ratio = std::min(
		requested_pixels / static_cast<double>(base_pixels), 1.0);

	auto target_aspect_segments = static_cast<uint32_t>(std::floor(
		std::sqrt(pixel_ratio * aspect_segments * aspect_segments)));
	for (auto i : {0, 1, -1}) {
		auto target_segments = std::max(
			static_cast<uint32_t>(1),
			std::min(aspect_segments, target_aspect_segments + i));
		auto output_width = aspect_width * target_segments;
		auto output_height = aspect_height * target_segments;

		if (output_width > ovi.base_width ||
		    output_height > ovi.base_height)
			continue;

		auto output_pixels = output_width * output_height;

		auto ratio =
			static_cast<float>(output_pixels) / requested_pixels;
		if (ratio < 0.9 || ratio > 1.1)
			continue;

		if (output_width % 4 != 0 ||
		    output_height % 2 !=
			    0) //libobs enforces multiple of 4 width and multiple of 2 height
			continue;

		blog(LOG_INFO,
		     "Scaled output resolution from %" PRIu64 "x%" PRIu64
		     " to %ux%u (base: %ux%u)",
		     requested_width, requested_height, output_width,
		     output_height, ovi.base_width, ovi.base_height);

		return {static_cast<uint32_t>(output_width),
			static_cast<uint32_t>(output_height)};
	}

	blog(LOG_WARNING,
	     "Failed to scale request resolution %" PRIu64 "x%" PRIu64
	     "u to %ux%u",
	     requested_width, requested_height, ovi.base_width,
	     ovi.base_height);

	return {static_cast<uint32_t>(requested_width),
		static_cast<uint32_t>(requested_height)};
}

static void adjust_video_encoder_scaling(const obs_video_info &ovi,
					 obs_encoder_t *video_encoder,
					 obs_data_t *encoder_config)
{
	uint64_t requested_width = obs_data_get_int(encoder_config, "width");
	uint64_t requested_height = obs_data_get_int(encoder_config, "height");

	if (ovi.output_width == requested_width ||
	    ovi.output_height == requested_height)
		return;

#if 0
	auto res = scale_resolution(ovi, requested_width, requested_height);
	obs_encoder_set_scaled_size(video_encoder, res.width, res.height);
#else
	obs_encoder_set_scaled_size(video_encoder,
				    static_cast<uint32_t>(requested_width),
				    static_cast<uint32_t>(requested_height));
#endif
	obs_encoder_set_gpu_scale_type(video_encoder, OBS_SCALE_BICUBIC);
}

static uint32_t closest_divisor(const obs_video_info &ovi,
				const media_frames_per_second &target_fps)
{
	auto target = (uint64_t)target_fps.numerator * ovi.fps_den;
	auto source = (uint64_t)ovi.fps_num * target_fps.denominator;
	return std::max(1u, static_cast<uint32_t>(source / target));
}

static void adjust_encoder_frame_rate_divisor(const obs_video_info &ovi,
					      obs_encoder_t *video_encoder,
					      obs_data_t *encoder_config,
					      const size_t encoder_index)
{
	media_frames_per_second requested_fps;
	const char *option = nullptr;
	if (!obs_data_get_frames_per_second(encoder_config, "framerate",
					    &requested_fps, &option)) {
		blog(LOG_WARNING, "`framerate` not specified for encoder %zu",
		     encoder_index);
		return;
	}

	if (ovi.fps_num == requested_fps.numerator &&
	    ovi.fps_den == requested_fps.denominator)
		return;

	auto divisor = closest_divisor(ovi, requested_fps);
	if (divisor <= 1)
		return;

	blog(LOG_INFO, "Setting frame rate divisor to %u for encoder %zu",
	     divisor, encoder_index);
	obs_encoder_set_frame_rate_divisor(video_encoder, divisor);
}

static OBSEncoderAutoRelease create_video_encoder(DStr &name_buffer,
						  size_t encoder_index,
						  obs_data_t *encoder_config)
{
	auto encoder_type = obs_data_get_string(encoder_config, "type");
	dstr_printf(name_buffer, "simulcast video encoder %zu", encoder_index);
	OBSEncoderAutoRelease video_encoder = obs_video_encoder_create(
		encoder_type, name_buffer, encoder_config, nullptr);
	if (!video_encoder) {
		blog(LOG_ERROR, "failed to create video encoder '%s'",
		     name_buffer->array);
		return nullptr;
	}
	obs_encoder_set_video(video_encoder, obs_get_video());

	obs_video_info ovi;
	if (!obs_get_video_info(&ovi)) {
		blog(LOG_WARNING,
		     "Failed to get obs video info while creating encoder %zu",
		     encoder_index);
		return nullptr;
	}

	adjust_video_encoder_scaling(ovi, video_encoder, encoder_config);
	adjust_encoder_frame_rate_divisor(ovi, video_encoder, encoder_config,
					  encoder_index);

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
		OBSDataAutoRelease encoder_config =
			obs_data_array_item(encoder_configs, i);
		auto encoder = create_video_encoder(video_encoder_name_buffer,
						    i, encoder_config);
		obs_output_set_video_encoder2(output, encoder, i);
		video_encoders_.emplace_back(std::move(encoder));
	}

	audio_encoder_ = create_audio_encoder();
	obs_output_set_audio_encoder(output, audio_encoder_, 0);

	return output;
}
