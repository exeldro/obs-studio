#include "simulcast-output.h"

#include <util/dstr.hpp>
#include <util/platform.h>
#include <util/profiler.hpp>
#include <util/util.hpp>
#include <obs-frontend-api.h>
#include <obs-module.h>
#include <obs.hpp>

#include <algorithm>
#include <cinttypes>
#include <cmath>
#include <numeric>
#include <optional>
#include <vector>

#include <QScopeGuard>
#include <QString>
#include <QThreadPool>

#include "plugin-macros.generated.h"
#include "qt-helpers.h"

static OBSServiceAutoRelease create_service(obs_data_t *go_live_config,
					    const QString &stream_key)
{
	const char *url = nullptr;
	OBSDataArrayAutoRelease ingest_endpoints =
		obs_data_get_array(go_live_config, "ingest_endpoints");
	for (size_t i = 0; i < obs_data_array_count(ingest_endpoints); i++) {
		OBSDataAutoRelease item =
			obs_data_array_item(ingest_endpoints, i);
		if (qstrnicmp("RTMP", obs_data_get_string(item, "protocol"), 4))
			continue;

		url = obs_data_get_string(item, "url_template");
		blog(LOG_INFO, "Using URL template: '%s'", url);
		break;
	}

	DStr str;
	dstr_cat(str, url);

	{
		auto found = dstr_find(str, "/{stream_key}");
		if (found)
			dstr_remove(str, found - str->array,
				    str->len - (found - str->array));
	}

	OBSDataAutoRelease settings = obs_data_create();
	obs_data_set_string(settings, "server", str->array);
	obs_data_set_string(settings, "key", stream_key.toUtf8().constData());

	auto service = obs_service_create(
		"simulcast_service", "simulcast service", settings, nullptr);

	if (!service) {
		blog(LOG_WARNING, "Failed to create simulcast service");
		throw QString::asprintf("Failed to create simulcast service");
	}

	return service;
}

static OBSOutputAutoRelease create_output()
{
	OBSOutputAutoRelease output = obs_output_create(
		"rtmp_output_simulcast", "rtmp simulcast", nullptr, nullptr);

	if (!output) {
		blog(LOG_ERROR, "failed to create simulcast rtmp output");
		throw QString::asprintf(
			"Failed to create simulcast rtmp output");
	}

	if (output) {
		obs_output_set_media(output, obs_get_video(), obs_get_audio());
	}

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
		static_cast<uint64_t>(ovi.base_width) * ovi.base_height;

	auto requested_pixels = requested_width * requested_height;

	auto pixel_ratio = std::min(
		requested_pixels / static_cast<double>(base_pixels), 1.0);

	auto target_aspect_segments = static_cast<uint32_t>(std::floor(
		std::sqrt(pixel_ratio * aspect_segments * aspect_segments)));

	if (target_aspect_segments + 1 * aspect_width > ovi.base_width)
		target_aspect_segments = ovi.base_width / aspect_width - 1;
	if (target_aspect_segments + 1 * aspect_height > ovi.base_height)
		target_aspect_segments = ovi.base_height / aspect_height - 1;

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

		if (output_width == requested_width &&
		    output_height == requested_height)
			return {static_cast<uint32_t>(output_width),
				static_cast<uint32_t>(requested_height)};

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
	     " to %ux%u",
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

#if 1
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

static const std::vector<const char *> &get_available_encoders()
{
	// encoders are currently only registered during startup, so keeping
	// a static vector around shouldn't be a problem
	static std::vector<const char *> available_encoders = [] {
		std::vector<const char *> available_encoders;
		for (size_t i = 0;; i++) {
			const char *id = nullptr;
			if (!obs_enum_encoder_types(i, &id))
				break;
			available_encoders.push_back(id);
		}
		return available_encoders;
	}();
	return available_encoders;
}

static bool encoder_available(const char *type)
{
	auto &encoders = get_available_encoders();
	return std::find_if(std::begin(encoders), std::end(encoders),
			    [=](const char *encoder) {
				    return strcmp(type, encoder) == 0;
			    }) != std::end(encoders);
}

static OBSEncoderAutoRelease create_video_encoder(DStr &name_buffer,
						  size_t encoder_index,
						  obs_data_t *encoder_config)
{
	auto encoder_type = obs_data_get_string(encoder_config, "type");
	if (!encoder_available(encoder_type)) {
		blog(LOG_ERROR, "Encoder type '%s' not available",
		     encoder_type);
		throw QString::asprintf(
			"NVENC not available.\n\nFailed to find encoder type '%s'", // FIXME: Remove 'NVENC' bit once we support other encoders
			encoder_type);
	}

	dstr_printf(name_buffer, "simulcast video encoder %zu", encoder_index);

	if (obs_data_has_user_value(encoder_config, "keyInt_sec") &&
	    !obs_data_has_user_value(encoder_config, "keyint_sec")) {
		blog(LOG_INFO,
		     "Fixing Go Live Config for encoder '%s': keyInt_sec -> keyint_sec",
		     name_buffer->array);
		obs_data_set_int(encoder_config, "keyint_sec",
				 obs_data_get_int(encoder_config,
						  "keyInt_sec"));
	}

	OBSEncoderAutoRelease video_encoder = obs_video_encoder_create(
		encoder_type, name_buffer, encoder_config, nullptr);
	if (!video_encoder) {
		blog(LOG_ERROR, "failed to create video encoder '%s'",
		     name_buffer->array);
		throw QString::asprintf(
			"Failed to create video encoder '%s' (type: '%s')",
			name_buffer->array, encoder_type);
	}
	obs_encoder_set_video(video_encoder, obs_get_video());

	obs_video_info ovi;
	if (!obs_get_video_info(&ovi)) {
		blog(LOG_WARNING,
		     "Failed to get obs video info while creating encoder %zu",
		     encoder_index);
		throw QString::asprintf(
			"Failed to get obs video info while creating encoder '%s' (type: '%s')",
			name_buffer->array, encoder_type);
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
		throw QString::asprintf("Failed to create audio encoder");
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

static OBSOutputAutoRelease
SetupOBSOutput(obs_data_t *go_live_config,
	       std::vector<OBSEncoderAutoRelease> &video_encoders,
	       OBSEncoderAutoRelease &audio_encoder);
static void SetupSignalHandlers(SimulcastOutput *self, obs_output_t *output);

struct OutputObjects {
	OBSOutputAutoRelease output;
	std::vector<OBSEncoderAutoRelease> video_encoders;
	OBSEncoderAutoRelease audio_encoder;
	OBSServiceAutoRelease simulcast_service;
};

QFuture<bool> SimulcastOutput::StartStreaming(const QString &stream_key,
					      obs_data_t *go_live_config_)
{
	OBSData go_live_config_data = go_live_config_;

	return CreateFuture()
		.then(QThreadPool::globalInstance(),
		      [stream_key, go_live_config_data, self = this,
		       video_encoders = std::move(video_encoders_)]() mutable
		      -> std::optional<OutputObjects> {
			      auto config = go_live_config_data
						    ? nullptr
						    : load_simulcast_config();
			      auto go_live_config =
				      go_live_config_data
					      ? static_cast<obs_data_t *>(
							go_live_config_data)
					      : &*config;
			      if (!go_live_config)
				      return std::nullopt;

			      video_encoders.clear();
			      OBSEncoderAutoRelease audio_encoder = nullptr;
			      auto output = SetupOBSOutput(go_live_config,
							   video_encoders,
							   audio_encoder);
			      if (!output)
				      return std::nullopt;

			      auto simulcast_service = create_service(
				      go_live_config, stream_key);
			      if (!simulcast_service)
				      return std::nullopt;

			      obs_output_set_service(output, simulcast_service);

			      return OutputObjects{
				      std::move(output),
				      std::move(video_encoders),
				      std::move(audio_encoder),
				      std::move(simulcast_service),
			      };
		      })
		.then(this,
		      [this](std::optional<OutputObjects> val) -> OBSOutput {
			      if (!val.has_value())
				      return nullptr;

			      auto vals = std::move(val).value();

			      SetupSignalHandlers(this, vals.output);

			      output_ = std::move(vals.output);
			      video_encoders_ = std::move(vals.video_encoders);
			      audio_encoder_ = std::move(vals.audio_encoder);
			      simulcast_service_ =
				      std::move(vals.simulcast_service);
			      return OBSOutput{output_};
		      })
		.then(QThreadPool::globalInstance(), [](OBSOutput output) {
			if (!output)
				return false;

			if (!obs_output_start(output)) {
				blog(LOG_WARNING, "Failed to start stream");
				throw QString::asprintf(
					"Failed to start stream (obs_output_start returned false)");
			}

			blog(LOG_INFO, "starting stream");
			return true;
		});
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

bool SimulcastOutput::IsStreaming() const
{
	return streaming_;
}

const std::vector<OBSEncoderAutoRelease> &SimulcastOutput::VideoEncoders() const
{
	return video_encoders_;
}

static OBSOutputAutoRelease
SetupOBSOutput(obs_data_t *go_live_config,
	       std::vector<OBSEncoderAutoRelease> &video_encoders,
	       OBSEncoderAutoRelease &audio_encoder)
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
		if (!encoder)
			return nullptr;

		obs_output_set_video_encoder2(output, encoder, i);
		video_encoders.emplace_back(std::move(encoder));
	}

	audio_encoder = create_audio_encoder();
	obs_output_set_audio_encoder(output, audio_encoder, 0);

	return output;
}

void SetupSignalHandlers(SimulcastOutput *self, obs_output_t *output)
{
	auto handler = obs_output_get_signal_handler(output);

	signal_handler_connect(handler, "start", StreamStartHandler, self);

	signal_handler_connect(handler, "stop", StreamStopHandler, self);
}

void StreamStartHandler(void *arg, calldata_t * /* data */)
{
	auto self = static_cast<SimulcastOutput *>(arg);
	self->streaming_ = true;
	emit self->StreamStarted();
}

void StreamStopHandler(void *arg, calldata_t * /* data */)
{
	auto self = static_cast<SimulcastOutput *>(arg);
	self->streaming_ = false;
	emit self->StreamStopped();
}
