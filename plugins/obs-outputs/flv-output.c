/******************************************************************************
    Copyright (C) 2023 by Lain Bailey <lain@obsproject.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#include <stdio.h>
#include <obs-module.h>
#include <obs-avc.h>
#include <util/platform.h>
#include <util/dstr.h>
#include <util/threading.h>
#include <inttypes.h>
#include "flv-mux.h"

#define do_log(level, format, ...)                \
	blog(level, "[flv output: '%s'] " format, \
	     obs_output_get_name(stream->output), ##__VA_ARGS__)

#define warn(format, ...) do_log(LOG_WARNING, format, ##__VA_ARGS__)
#define info(format, ...) do_log(LOG_INFO, format, ##__VA_ARGS__)

struct flv_output {
	obs_output_t *output;
	struct dstr path;
	FILE *file;
	volatile bool active;
	volatile bool stopping;
	uint64_t stop_ts;
	bool sent_headers;
	int64_t last_packet_ts;

	flv_additional_meta_data_t additional_metadata;

	pthread_mutex_t mutex;

	bool got_first_video;
	int32_t start_dts_offset;
};

static inline bool stopping(struct flv_output *stream)
{
	return os_atomic_load_bool(&stream->stopping);
}

static inline bool active(struct flv_output *stream)
{
	return os_atomic_load_bool(&stream->active);
}

static const char *flv_output_getname(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("FLVOutput");
}

static void flv_output_stop(void *data, uint64_t ts);

static void flv_output_destroy(void *data)
{
	struct flv_output *stream = data;

	pthread_mutex_destroy(&stream->mutex);
	dstr_free(&stream->path);
	bfree(stream);
}

static void *flv_output_create(obs_data_t *settings, obs_output_t *output)
{
	struct flv_output *stream = bzalloc(sizeof(struct flv_output));
	stream->output = output;
	pthread_mutex_init(&stream->mutex, NULL);

	UNUSED_PARAMETER(settings);
	return stream;
}

static int write_packet(struct flv_output *stream,
			struct encoder_packet *packet, bool is_header,
			size_t idx)
{
	uint8_t *data;
	size_t size;
	int ret = 0;

	flv_additional_media_data_t *media_data =
		packet->type == OBS_ENCODER_AUDIO
			? &stream->additional_metadata
				   .additional_audio_media_data[idx]
			: &stream->additional_metadata
				   .additional_video_media_data[idx];

	stream->last_packet_ts = get_ms_time(packet, packet->dts);

	if (media_data->active) {
		flv_additional_packet_mux(
			packet, is_header ? 0 : stream->start_dts_offset, &data,
			&size, is_header, media_data);
	} else {
		flv_packet_mux(packet, is_header ? 0 : stream->start_dts_offset,
			       &data, &size, is_header);
	}
	fwrite(data, 1, size, stream->file);
	bfree(data);

	return ret;
}

static void write_additional_meta_data(struct flv_output *stream)
{
	uint8_t *meta_data;
	size_t meta_data_size;

	if (stream->additional_metadata.processing_intents.num <= 0)
		return;

	flv_additional_meta_data(stream->output, &stream->additional_metadata,
				 &meta_data, &meta_data_size);
	fwrite(meta_data, 1, meta_data_size, stream->file);
	bfree(meta_data);
}

static void write_meta_data(struct flv_output *stream)
{
	uint8_t *meta_data;
	size_t meta_data_size;

	flv_meta_data(stream->output, &meta_data, &meta_data_size, true);
	fwrite(meta_data, 1, meta_data_size, stream->file);
	bfree(meta_data);
}

static bool write_audio_header(struct flv_output *stream, size_t idx)
{
	obs_output_t *context = stream->output;
	obs_encoder_t *aencoder = obs_output_get_audio_encoder(context, idx);

	struct encoder_packet packet = {.type = OBS_ENCODER_AUDIO,
					.timebase_den = 1};

	if (!aencoder)
		return false;

	if (obs_encoder_get_extra_data(aencoder, &packet.data, &packet.size))
		write_packet(stream, &packet, true, idx);

	return true;
}

static bool write_video_header(struct flv_output *stream, size_t idx)
{
	obs_output_t *context = stream->output;
	obs_encoder_t *vencoder = obs_output_get_video_encoder2(context, idx);
	uint8_t *header;
	size_t size;

	struct encoder_packet packet = {.type = OBS_ENCODER_VIDEO,
					.timebase_den = 1,
					.keyframe = true};

	if (!vencoder)
		return false;

	if (obs_encoder_get_extra_data(vencoder, &header, &size)) {
		packet.size = obs_parse_avc_header(&packet.data, header, size);
		write_packet(stream, &packet, true, idx);
		bfree(packet.data);
	}

	return true;
}

static void add_processing_intents(struct flv_output *stream);

static void write_headers(struct flv_output *stream)
{
	add_processing_intents(stream);
	write_meta_data(stream);
	write_additional_meta_data(stream);
	write_video_header(stream, 0);
	write_audio_header(stream, 0);

	for (size_t i = 1; write_audio_header(stream, i); i++)
		;
	for (size_t i = 1; write_video_header(stream, i); i++)
		;
}

static void add_processing_intents(struct flv_output *stream)
{
	obs_encoder_t *venc = NULL;
	obs_encoder_t *aenc = NULL;
	bool additional_audio = false;
	bool additional_video = false;

	for (size_t i = 0; i < MAX_OUTPUT_VIDEO_ENCODERS; i++) {
		obs_encoder_t *enc =
			obs_output_get_video_encoder2(stream->output, i);
		if (enc && !venc) {
			venc = enc;
			continue;
		}

		if (enc && enc != venc) {
			additional_video = true;
			break;
		}
	}

	for (size_t i = 0; i < MAX_OUTPUT_AUDIO_ENCODERS; i++) {
		obs_encoder_t *enc =
			obs_output_get_audio_encoder(stream->output, i);
		if (enc && !aenc) {
			aenc = enc;
			continue;
		}

		if (enc && enc != aenc) {
			additional_audio = true;
			break;
		}
	}

	int stream_index = 0;
	if (additional_audio) {
		// Add our processing intent for audio
		char *intent = bstrdup("ArchiveProgramNarrationAudio");
		da_push_back(stream->additional_metadata.processing_intents,
			     &intent);

		for (size_t i = 0; i < MAX_OUTPUT_AUDIO_ENCODERS; i++) {
			obs_encoder_t *enc =
				obs_output_get_audio_encoder(stream->output, i);
			flv_additional_media_data_t *amd =
				&stream->additional_metadata
					 .additional_audio_media_data[i];

			// Skip primary audio or null encoders
			if (!enc || enc == aenc)
				continue;

			amd->active = true;

			dstr_printf(&amd->stream_name, "stream%d",
				    stream_index++);
			flv_media_label_t content_type =
				flv_media_label_create_string("contentType",
							      "PNAR");
			da_push_back(amd->media_labels, &content_type);
		}
	}

	if (additional_video) {
		// Add our processing intent for video
		char *intent = bstrdup("SimulcastVideo");
		da_push_back(stream->additional_metadata.processing_intents,
			     &intent);

		for (size_t i = 0; i < MAX_OUTPUT_VIDEO_ENCODERS; i++) {
			obs_encoder_t *enc = obs_output_get_video_encoder2(
				stream->output, i);
			flv_additional_media_data_t *amd =
				&stream->additional_metadata
					 .additional_video_media_data[i];

			// Skip primary video or null encoders
			if (!enc || enc == venc)
				continue;

			amd->active = true;

			dstr_printf(&amd->stream_name, "stream%d",
				    stream_index++);
		}
	}

	flv_media_label_t audio_content_type =
		flv_media_label_create_string("contentType", "PRM");
	flv_media_label_t video_content_type =
		flv_media_label_create_string("contentType", "PRM");

	da_push_back(stream->additional_metadata.default_audio_media_labels,
		     &audio_content_type);
	da_push_back(stream->additional_metadata.default_video_media_labels,
		     &video_content_type);
}

static bool flv_output_start(void *data)
{
	struct flv_output *stream = data;
	obs_data_t *settings;
	const char *path;

	if (!obs_output_can_begin_data_capture(stream->output, 0))
		return false;
	if (!obs_output_initialize_encoders(stream->output, 0))
		return false;

	stream->got_first_video = false;
	stream->sent_headers = false;
	os_atomic_set_bool(&stream->stopping, false);

	flv_additional_meta_data_free(&stream->additional_metadata);
	flv_additional_meta_data_init(&stream->additional_metadata);

	/* get path */
	settings = obs_output_get_settings(stream->output);
	path = obs_data_get_string(settings, "path");
	dstr_copy(&stream->path, path);
	obs_data_release(settings);

	stream->file = os_fopen(stream->path.array, "wb");
	if (!stream->file) {
		warn("Unable to open FLV file '%s'", stream->path.array);
		return false;
	}

	/* write headers and start capture */
	os_atomic_set_bool(&stream->active, true);
	obs_output_begin_data_capture(stream->output, 0);

	info("Writing FLV file '%s'...", stream->path.array);
	return true;
}

static void flv_output_stop(void *data, uint64_t ts)
{
	struct flv_output *stream = data;
	stream->stop_ts = ts / 1000;
	os_atomic_set_bool(&stream->stopping, true);
}

static void flv_output_actual_stop(struct flv_output *stream, int code)
{
	os_atomic_set_bool(&stream->active, false);

	if (stream->file) {
		write_file_info(stream->file, stream->last_packet_ts,
				os_ftelli64(stream->file));

		fclose(stream->file);
	}
	if (code) {
		obs_output_signal_stop(stream->output, code);
	} else {
		obs_output_end_data_capture(stream->output);
	}

	flv_additional_meta_data_free(&stream->additional_metadata);

	info("FLV file output complete");
}

static void flv_output_data(void *data, struct encoder_packet *packet)
{
	struct flv_output *stream = data;
	struct encoder_packet parsed_packet;

	pthread_mutex_lock(&stream->mutex);

	if (!active(stream))
		goto unlock;

	if (!packet) {
		flv_output_actual_stop(stream, OBS_OUTPUT_ENCODE_ERROR);
		goto unlock;
	}

	if (stopping(stream)) {
		if (packet->sys_dts_usec >= (int64_t)stream->stop_ts) {
			flv_output_actual_stop(stream, 0);
			goto unlock;
		}
	}

	if (!stream->sent_headers) {
		write_headers(stream);
		stream->sent_headers = true;
	}

	if (packet->type == OBS_ENCODER_VIDEO) {
		if (!stream->got_first_video) {
			stream->start_dts_offset =
				get_ms_time(packet, packet->dts);
			stream->got_first_video = true;
		}

		obs_parse_avc_packet(&parsed_packet, packet);
		write_packet(stream, &parsed_packet, false, packet->track_idx);
		obs_encoder_packet_release(&parsed_packet);
	} else {
		write_packet(stream, packet, false, packet->track_idx);
	}

unlock:
	pthread_mutex_unlock(&stream->mutex);
}

static obs_properties_t *flv_output_properties(void *unused)
{
	UNUSED_PARAMETER(unused);

	obs_properties_t *props = obs_properties_create();

	obs_properties_add_text(props, "path",
				obs_module_text("FLVOutput.FilePath"),
				OBS_TEXT_DEFAULT);
	return props;
}

struct obs_output_info flv_output_info = {
	.id = "flv_output",
	.flags = OBS_OUTPUT_AV | OBS_OUTPUT_ENCODED | OBS_OUTPUT_MULTI_TRACK_AV,
	.encoded_video_codecs = "h264",
	.encoded_audio_codecs = "aac",
	.get_name = flv_output_getname,
	.create = flv_output_create,
	.destroy = flv_output_destroy,
	.start = flv_output_start,
	.stop = flv_output_stop,
	.encoded_packet = flv_output_data,
	.get_properties = flv_output_properties,
};
