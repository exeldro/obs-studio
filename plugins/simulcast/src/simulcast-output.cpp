#include "simulcast-output.h"

#include <util/dstr.hpp>
#include <obs-frontend-api.h>

#include "plugin-macros.generated.h"

void SimulcastOutput::StartStreaming()
{
	{
		output_ = obs_output_create("rtmp_output_simulcast",
					    "rtmp simulcast", nullptr, nullptr);
		obs_output_set_service(output_,
				       obs_frontend_get_streaming_service());
		if (!output_) {
			blog(LOG_ERROR,
			     "failed to create simulcast rtmp output");
			return;
		}
	}

	{
		DStr name_buffer;
		for (size_t i = 0; i < 1; i++) {
			dstr_printf(name_buffer, "simulcast video encoder %z",
				    i);
			auto video_encoder = obs_video_encoder_create(
				"jim_nvenc", name_buffer, nullptr, nullptr);
			if (!video_encoder) {
				blog(LOG_ERROR,
				     "failed to create video encoder '%s'",
				     name_buffer->array);
				return;
			}
			video_encoders_.push_back(video_encoder);
			obs_output_set_video_encoder2(output_, video_encoder,
						      i);
		}
	}

	{
		audio_encoder_ = obs_audio_encoder_create(
			"ffmpeg_aac", "simulcast aac", nullptr, 0, nullptr);
		if (!audio_encoder_) {
			blog(LOG_ERROR, "failed to create audio encoder");
			return;
		}
		obs_output_set_audio_encoder(output_, audio_encoder_, 0);
	}

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
