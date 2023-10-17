#pragma once

#include <obs.hpp>

#include <atomic>
#include <optional>
#include <vector>

#include <qobject.h>
#include <QFuture>

#include "immutable-date-time.h"

class SimulcastOutput;
class QString;

void StreamStartHandler(void *arg, calldata_t *data);
void StreamStopHandler(void *arg, calldata_t *data);

void RecordingStartHandler(void *arg, calldata_t *data);
void RecordingStopHandler(void *arg, calldata_t *data);

class SimulcastOutput : public QObject {
	Q_OBJECT;

public:
	bool PrepareStreaming(QWidget *parent, const QString &device_id,
			      const QString &obs_session_id,
			      const QString &rtmp_url,
			      const QString &stream_key,
			      bool use_ertmp_multitrack);
	signal_handler_t *StreamingSignalHandler();
	bool StartStreaming();
	void StopStreaming();
	bool IsStreaming() const;
	std::optional<int> ConnectTimeMs() const;

	bool StartRecording(obs_data_t *go_live_config,
			    bool use_ertmp_multitrack);
	void StopRecording();
	bool IsRecording() const { return recording_; }

	const std::vector<OBSEncoderAutoRelease> &VideoEncoders() const;

	obs_output_t *StreamingOutput() { return output_; }

signals:
	void StreamStarted();
	void StreamStopped();

	void RecordingStarted();
	void RecordingStopped();

private:
	const ImmutableDateTime &GenerateStreamAttemptStartTime();

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
