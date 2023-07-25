#include "simulcast-output.h"

#include <util/dstr.hpp>
#include <obs-frontend-api.h>

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
	dstr_printf(name_buffer, "simulcast video encoder %z", encoder_index);
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

void SimulcastOutput::StartStreaming()
{
	output_ = create_output();

	{
		DStr name_buffer;
		for (size_t i = 0; i < 1; i++) {
			auto video_encoder =
				create_video_encoder(name_buffer, i);
			obs_output_set_video_encoder2(output_, video_encoder,
						      i);
			video_encoders_.push_back(std::move(video_encoder));
		}
	}

	audio_encoder_ = create_audio_encoder();
	obs_output_set_audio_encoder(output_, audio_encoder_, 0);

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
