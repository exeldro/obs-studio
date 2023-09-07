#pragma once

#include "simulcast-output.h"
#include "immutable-date-time.h"
#include "berryessa-submitter.hpp"
#include "berryessa-every-minute.hpp"

#include <QAction>
#include <QByteArray>
#include <QString>
#include <QWidget>

#include <memory>
#include <optional>

#include <util/dstr.hpp>

class SimulcastDockWidget : public QWidget {
	Q_OBJECT;

public:
	SimulcastDockWidget(obs_data_t *settings_config, QWidget *parent = 0);

	void SaveConfig();
	void LoadConfig();
	void ProfileRenamed();
	void PruneDeletedProfiles();

	void CheckPromptToMakeDockVisible();

	void OpenSettings();
	void SetOpenSettingsAction(QAction *action)
	{
		open_settings_action_ = action;
	}

	SimulcastOutput &Output() { return output_; }

	const QString &OBSSessionId() { return obs_session_id_; }

	QString &RTMPURL() { return rtmp_url_; }
	QString &StreamKey() { return stream_key_; }
	bool &TelemetryEanbled() { return telemetry_enabled_; }
	bool &UseServerConfig() { return use_server_config_; }
	QString &CustomConfig() { return custom_config_; }
	const QString &DeviceId() { return device_id_; }
	QByteArray &SettingsWindowGeometry()
	{
		return settings_window_geometry_;
	}

	const ImmutableDateTime &GenerateStreamAttemptStartTime();
	const std::optional<ImmutableDateTime> &StreamAttemptStartTime() const;

	void SetParentStyleSheet(obs_data_t *dock_config);

signals:
	void ProfileChanged();

private:
	SimulcastOutput output_;
	BerryessaSubmitter berryessa_;
	std::unique_ptr<BerryessaEveryMinute> berryessaEveryMinute_;

	QPointer<QAction> open_settings_action_;

	const bool override_rtmp_url_;

	QString obs_session_id_;

	// Add config vars here
	QString rtmp_url_;
	QString stream_key_;
	bool telemetry_enabled_;
	bool use_server_config_;
	QString custom_config_;
	QString device_id_;
	QByteArray settings_window_geometry_;
	std::optional<QDateTime> make_dock_visible_prompt_;
	// Add config vars above

	OBSDataAutoRelease config_;
	OBSDataAutoRelease profiles_;
	DStr profile_name_;
	OBSDataAutoRelease profile_;

	std::optional<ImmutableDateTime> stream_attempt_start_time_;
};
