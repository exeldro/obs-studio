#pragma once

#include "simulcast-output.h"
#include "immutable-date-time.h"
#include "berryessa-submitter.hpp"
#include "berryessa-every-minute.hpp"

#include <QByteArray>
#include <QString>
#include <QWidget>

#include <memory>
#include <optional>

#include <util/dstr.hpp>

class SimulcastDockWidget : public QWidget {
	Q_OBJECT;

public:
	SimulcastDockWidget(QWidget *parent = 0);

	void SaveConfig();
	void LoadConfig();
	void ProfileRenamed();
	void PruneDeletedProfiles();

	void CheckPromptToMakeDockVisible();

	SimulcastOutput &Output() { return output_; }

	QString &StreamKey() { return stream_key_; }
	bool &TelemetryEanbled() { return telemetry_enabled_; }
	QByteArray &SettingsWindowGeometry()
	{
		return settings_window_geometry_;
	}

	const ImmutableDateTime &GenerateStreamAttemptStartTime();
	const std::optional<ImmutableDateTime> &StreamAttemptStartTime() const;

signals:
	void ProfileChanged();

private:
	SimulcastOutput output_;
	BerryessaSubmitter berryessa_;
	std::unique_ptr<BerryessaEveryMinute> berryessaEveryMinute_;

	// Add config vars here
	QString stream_key_;
	bool telemetry_enabled_;
	QByteArray settings_window_geometry_;
	std::optional<QDateTime> make_dock_visible_prompt_;
	// Add config vars above

	OBSDataAutoRelease config_;
	OBSDataAutoRelease profiles_;
	DStr profile_name_;
	OBSDataAutoRelease profile_;

	std::optional<ImmutableDateTime> stream_attempt_start_time_;
};
