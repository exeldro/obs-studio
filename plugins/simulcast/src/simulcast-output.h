#pragma once

#include <obs.hpp>

#include <atomic>
#include <optional>
#include <vector>

#include <qobject.h>
#include <QFuture>

class SimulcastOutput;
class QString;

void StreamStartHandler(void *arg, calldata_t *data);
void StreamStopHandler(void *arg, calldata_t *data);

class SimulcastOutput : public QObject {
	Q_OBJECT;

public:
	QFuture<bool> StartStreaming(const QString &device_id,
				     const QString &obs_session_id,
				     const QString &rtmp_url,
				     const QString &stream_key,
				     obs_data_t *go_live_config);
	void StopStreaming();
	bool IsStreaming() const;
	std::optional<int> ConnectTimeMs() const;

	const std::vector<OBSEncoderAutoRelease> &VideoEncoders() const;

signals:
	void StreamStarted();
	void StreamStopped();

private:
	std::atomic<bool> streaming_ = false;

	OBSOutputAutoRelease output_;
	OBSWeakOutputAutoRelease weak_output_;
	std::vector<OBSEncoderAutoRelease> video_encoders_;
	OBSEncoderAutoRelease audio_encoder_;
	OBSServiceAutoRelease simulcast_service_;

	friend void StreamStartHandler(void *arg, calldata_t *data);
	friend void StreamStopHandler(void *arg, calldata_t *data);
};
