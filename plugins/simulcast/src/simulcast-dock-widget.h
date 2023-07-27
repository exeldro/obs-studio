#pragma once

#include "simulcast-output.h"

#include <QString>
#include <QWidget>

#include <util/dstr.hpp>

class SimulcastDockWidget : public QWidget {
public:
	SimulcastDockWidget(QWidget *parent = 0);

	void SaveConfig();
	void LoadConfig();

	SimulcastOutput &Output() { return output_; }

	QString &StreamKey() { return stream_key_; }

private:
	SimulcastOutput output_;

	// Add config vars here
	QString stream_key_;
	// Add config vars above

	OBSDataAutoRelease config_;
	OBSDataAutoRelease profiles_;
	DStr profile_name_;
	OBSDataAutoRelease profile_;
};
