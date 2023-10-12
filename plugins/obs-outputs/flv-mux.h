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

#pragma once

#include <obs.h>

#define MILLISECOND_DEN 1000
#define FLV_ADDITIONAL_MEDIA_DATA_CODEC_ID_H264 7

enum video_id_t {
	CODEC_H264 = 1, // legacy & Y2023 spec
	CODEC_AV1,      // Y2023 spec
	CODEC_HEVC,
};

typedef enum {
	FLV_MEDIA_LABEL_TYPE_UNKNOWN,
	FLV_MEDIA_LABEL_TYPE_STRING,
	FLV_MEDIA_LABEL_TYPE_NUMBER,
} flv_media_label_type_t;

typedef struct {
	const char *property;
	void *value;
	flv_media_label_type_t type;
} flv_media_label_t;

typedef struct {
	bool active;
	enum obs_encoder_type type;
	struct dstr stream_name;
	DARRAY(flv_media_label_t) media_labels;
} flv_additional_media_data_t;

typedef struct {
	DARRAY(char *) processing_intents;
	DARRAY(flv_media_label_t) default_audio_media_labels;
	DARRAY(flv_media_label_t) default_video_media_labels;
	flv_additional_media_data_t
		additional_audio_media_data[MAX_OUTPUT_AUDIO_ENCODERS];
	flv_additional_media_data_t
		additional_video_media_data[MAX_OUTPUT_VIDEO_ENCODERS];
} flv_additional_meta_data_t;

static enum video_id_t to_video_type(const char *codec)
{
	if (strcmp(codec, "h264") == 0)
		return CODEC_H264;
	if (strcmp(codec, "av1") == 0)
		return CODEC_AV1;
	if (strcmp(codec, "hevc") == 0)
		return CODEC_HEVC;
	return 0;
}

static int32_t get_ms_time(struct encoder_packet *packet, int64_t val)
{
	return (int32_t)(val * MILLISECOND_DEN / packet->timebase_den);
}

extern void write_file_info(FILE *file, int64_t duration_ms, int64_t size);

extern void flv_meta_data(obs_output_t *context, uint8_t **output, size_t *size,
			  bool write_header);
extern void
flv_additional_meta_data(obs_output_t *context,
			 flv_additional_meta_data_t *additional_meta_data,
			 uint8_t **data, size_t *size);
extern void flv_packet_mux(struct encoder_packet *packet, int32_t dts_offset,
			   uint8_t **output, size_t *size, bool is_header);
extern void
flv_additional_packet_mux(struct encoder_packet *packet, int32_t dts_offset,
			  uint8_t **output, size_t *size, bool is_header,
			  flv_additional_media_data_t *additional_meta_data);

static inline flv_media_label_t
flv_media_label_create_string(const char *property, char *value)
{
	flv_media_label_t label = {0};
	label.property = property;
	label.value = bstrdup(value);
	label.type = FLV_MEDIA_LABEL_TYPE_STRING;

	return label;
}

static inline flv_media_label_t
flv_media_label_create_number(const char *property, double value)
{
	double *val = bmalloc(sizeof(double));
	*val = value;

	flv_media_label_t label = {0};
	label.property = property;
	label.value = val;
	label.type = FLV_MEDIA_LABEL_TYPE_NUMBER;

	return label;
}

static inline const char *flv_media_label_get_string(flv_media_label_t *label)
{
	assert(label->type == FLV_MEDIA_LABEL_TYPE_STRING);

	return (const char *)label->value;
}

static inline double flv_media_label_get_number(flv_media_label_t *label)
{
	assert(label->type == FLV_MEDIA_LABEL_TYPE_NUMBER);

	return *((double *)label->value);
}

static inline void flv_media_label_free(flv_media_label_t *label)
{
	bfree(label->value);
	memset(label, 0, sizeof(flv_media_label_t));
}

static inline void
flv_additional_media_data_init(flv_additional_media_data_t *media_data)
{
	da_init(media_data->media_labels);
}

static inline void
flv_additional_media_data_free(flv_additional_media_data_t *media_data)
{
	for (size_t i = 0; i < media_data->media_labels.num; i++) {
		flv_media_label_free(&media_data->media_labels.array[i]);
	}

	da_free(media_data->media_labels);
	dstr_free(&media_data->stream_name);

	memset(media_data, 0, sizeof(flv_additional_media_data_t));
}

static inline void
flv_additional_meta_data_init(flv_additional_meta_data_t *meta_data)
{
	da_init(meta_data->processing_intents);

	for (size_t i = 0; i < MAX_OUTPUT_AUDIO_ENCODERS; i++) {
		flv_additional_media_data_init(
			&meta_data->additional_audio_media_data[i]);
		meta_data->additional_audio_media_data[i].type =
			OBS_ENCODER_AUDIO;
	}
	for (size_t i = 0; i < MAX_OUTPUT_VIDEO_ENCODERS; i++) {
		flv_additional_media_data_init(
			&meta_data->additional_video_media_data[i]);
		meta_data->additional_video_media_data[i].type =
			OBS_ENCODER_VIDEO;
	}
}

static inline void
flv_additional_meta_data_free(flv_additional_meta_data_t *meta_data)
{
	for (size_t i = 0; i < MAX_OUTPUT_AUDIO_ENCODERS; i++) {
		flv_additional_media_data_free(
			&meta_data->additional_audio_media_data[i]);
	}

	for (size_t i = 0; i < MAX_OUTPUT_VIDEO_ENCODERS; i++) {
		flv_additional_media_data_free(
			&meta_data->additional_video_media_data[i]);
	}

	for (size_t i = 0; i < meta_data->default_audio_media_labels.num; i++) {
		flv_media_label_free(
			&meta_data->default_audio_media_labels.array[i]);
	}
	for (size_t i = 0; i < meta_data->default_video_media_labels.num; i++) {
		flv_media_label_free(
			&meta_data->default_video_media_labels.array[i]);
	}
	for (size_t i = 0; i < meta_data->processing_intents.num; i++) {
		bfree(meta_data->processing_intents.array[i]);
	}

	da_free(meta_data->default_audio_media_labels);
	da_free(meta_data->default_video_media_labels);
	da_free(meta_data->processing_intents);

	memset(meta_data, 0, sizeof(flv_additional_meta_data_t));
}

// Y2023 spec
extern void flv_packet_start(struct encoder_packet *packet,
			     enum video_id_t codec, uint8_t **output,
			     size_t *size, size_t idx);
extern void flv_packet_frames(struct encoder_packet *packet,
			      enum video_id_t codec, int32_t dts_offset,
			      uint8_t **output, size_t *size, size_t idx);
extern void flv_packet_end(struct encoder_packet *packet, enum video_id_t codec,
			   uint8_t **output, size_t *size, size_t idx);
extern void flv_packet_metadata(enum video_id_t codec, uint8_t **output,
				size_t *size, int bits_per_raw_sample,
				uint8_t color_primaries, int color_trc,
				int color_space, int min_luminance,
				int max_luminance, size_t idx);

extern void flv_additional_packet_start_ex(
	struct encoder_packet *packet, flv_additional_media_data_t *media_data,
	enum video_id_t codec, uint8_t **output, size_t *size);
extern void
flv_additional_packet_frames_ex(struct encoder_packet *packet,
				flv_additional_media_data_t *media_data,
				enum video_id_t codec, int32_t dts_offset,
				uint8_t **output, size_t *size);
extern void flv_additional_packet_end_ex(
	struct encoder_packet *packet, flv_additional_media_data_t *media_data,
	enum video_id_t codec, uint8_t **output, size_t *size);
extern void flv_additional_packet_metadata_ex(
	flv_additional_media_data_t *media_data, enum video_id_t codec_id,
	uint8_t **output, size_t *size, int bits_per_raw_sample,
	uint8_t color_primaries, int color_trc, int color_space,
	int min_luminance, int max_luminance);
