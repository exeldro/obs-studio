#pragma once

#include <obs.hpp>

#include <vector>

#include <qobject.h>

class SimulcastOutput;
class QString;

void StreamStartHandler(void *arg, calldata_t *data);
void StreamStopHandler(void *arg, calldata_t *data);

class SimulcastOutput : public QObject {
	Q_OBJECT;

public:
	bool StartStreaming(const QString &stream_key,
			    obs_data_t *go_live_config);
	void StopStreaming();
	bool IsStreaming();

signals:
	void StreamStarted();
	void StreamStopped();

private:
	OBSOutputAutoRelease SetupOBSOutput(obs_data_t *go_live_config);
	void SetupSignalHandlers(obs_output_t *output);
	bool streaming_ = false;

	OBSOutputAutoRelease output_;
	std::vector<OBSEncoderAutoRelease> video_encoders_;
	OBSEncoderAutoRelease audio_encoder_;
	OBSServiceAutoRelease simulcast_service_;

	friend void StreamStartHandler(void *arg, calldata_t *data);
	friend void StreamStopHandler(void *arg, calldata_t *data);
};
