#pragma once

#include "simulcast-output.h"
#include "berryessa-submitter.hpp"
#include "berryessa-every-minute.hpp"

#include <QByteArray>
#include <QString>
#include <QWidget>

#include <memory>

#include <util/dstr.hpp>

class SimulcastDockWidget : public QWidget {
	Q_OBJECT;

public:
	SimulcastDockWidget(QWidget *parent = 0);

	void SaveConfig();
	void LoadConfig();
	void ProfileRenamed();
	void PruneDeletedProfiles();

	SimulcastOutput &Output() { return output_; }

	QString &StreamKey() { return stream_key_; }
	QByteArray &SettingsWindowGeometry()
	{
		return settings_window_geometry_;
	}

signals:
	void ProfileChanged();

private:
	SimulcastOutput output_;
	BerryessaSubmitter berryessa_;
	std::unique_ptr<BerryessaEveryMinute> berryessaEveryMinute_;

	// Add config vars here
	QString stream_key_;
	QByteArray settings_window_geometry_;
	// Add config vars above

	OBSDataAutoRelease config_;
	OBSDataAutoRelease profiles_;
	DStr profile_name_;
	OBSDataAutoRelease profile_;
};
