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

#ifndef SIMULCAST_OVERRIDE_RTMP_URL
#define SIMULCAST_OVERRIDE_RTMP_URL false
#endif

class SimulcastDockWidget : public QWidget {
	Q_OBJECT;

public:
	SimulcastDockWidget(QWidget *parent = 0);

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
	std::optional<uint64_t> &PreferenceMaximumBitrate()
	{
		return preference_maximum_bitrate_;
	}
	std::optional<uint32_t> &PreferenceMaximumRenditions()
	{
		return preference_maximum_renditions_;
	}
	bool &TelemetryEanbled() { return telemetry_enabled_; }
	bool &UseERTMPMultitrack() { return use_ertmp_multitrack_; }
	bool &UseServerConfig() { return use_server_config_; }
	QString &CustomConfig() { return custom_config_; }
	const QString &DeviceId() { return device_id_; }
	QByteArray &SettingsWindowGeometry()
	{
		return settings_window_geometry_;
	}

	const ImmutableDateTime &GenerateStreamAttemptStartTime();
	const std::optional<ImmutableDateTime> &StreamAttemptStartTime() const;

	void SetParentStyleSheet();

signals:
	void ProfileChanged();

private:
	SimulcastOutput output_;
	BerryessaSubmitter berryessa_;
	std::unique_ptr<BerryessaEveryMinute> berryessaEveryMinute_;

	QPointer<QAction> open_settings_action_;

	const bool override_rtmp_url_ = SIMULCAST_OVERRIDE_RTMP_URL;

	QString obs_session_id_;

	// Add config vars here
	QString rtmp_url_;
	QString stream_key_;
	std::optional<uint64_t> preference_maximum_bitrate_;
	std::optional<uint32_t> preference_maximum_renditions_;
	bool telemetry_enabled_;
	bool use_ertmp_multitrack_;
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
