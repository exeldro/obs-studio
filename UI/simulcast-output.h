#pragma once

#include <obs.hpp>

#include <atomic>
#include <optional>
#include <vector>

#include <qobject.h>
#include <QFuture>

#define NOMINMAX

#include "immutable-date-time.h"

#include "berryessa-submitter.hpp"
#include "berryessa-every-minute.hpp"

class QString;

void StreamStartHandler(void *arg, calldata_t *data);
void StreamStopHandler(void *arg, calldata_t *data);

void RecordingStartHandler(void *arg, calldata_t *data);
void RecordingStopHandler(void *arg, calldata_t *data);

struct SimulcastOutput {

public:
	void PrepareStreaming(QWidget *parent, const QString &rtmp_url,
			      const QString &stream_key,
			      bool use_ertmp_multitrack);
	signal_handler_t *StreamingSignalHandler();
	void StartedStreaming(QWidget *parent, bool success);
	void StopStreaming();
	bool IsStreaming() const;
	std::optional<int> ConnectTimeMs() const;

	bool StartRecording(obs_data_t *go_live_config,
			    bool use_ertmp_multitrack);
	void StopRecording();
	bool IsRecording() const { return recording_; }

	const std::vector<OBSEncoderAutoRelease> &VideoEncoders() const;

	obs_output_t *StreamingOutput() { return output_; }

private:
	const ImmutableDateTime &GenerateStreamAttemptStartTime();

	std::unique_ptr<BerryessaSubmitter> berryessa_;
	std::unique_ptr<BerryessaEveryMinute> berryessa_every_minute_;

	std::function<void(bool success, std::optional<int> connect_time_ms)>
		send_start_event;

	std::atomic<bool> streaming_ = false;
	std::atomic<bool> recording_ = false;

	std::optional<ImmutableDateTime> stream_attempt_start_time_;

	OBSOutputAutoRelease output_;
	OBSWeakOutputAutoRelease weak_output_;
	std::vector<OBSEncoderAutoRelease> video_encoders_;
	OBSEncoderAutoRelease audio_encoder_;
	OBSServiceAutoRelease simulcast_service_;

	OBSOutputAutoRelease recording_output_;
	OBSWeakOutputAutoRelease weak_recording_output_;

	friend void StreamStartHandler(void *arg, calldata_t *data);
	friend void StreamStopHandler(void *arg, calldata_t *data);
	friend void RecordingStartHandler(void *arg, calldata_t *data);
	friend void RecordingStopHandler(void *arg, calldata_t *data);
};
