#include "simulcast-dock-widget.h"

#include "simulcast-output.h"

#include <QMainWindow>
#include <QDockWidget>
#include <QWidget>
#include <QLabel>
#include <QString>
#include <QPushButton>
#include <QScrollArea>
#include <QGridLayout>
#include <QEvent>
#include <QThread>
#include <QLineEdit>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QMessageBox>
#include <QComboBox>
#include <QCheckBox>
#include <QGroupBox>
#include <QAction>

#include <obs.h>
#include <obs-frontend-api.h>
#include <obs-module.h>
#include <util/config-file.h>
#include <util/platform.h>
#include <util/util.hpp>

#define ConfigSection "simulcast"

static void SetupSignalsAndSlots(SimulcastDockWidget *self,
				 QPushButton *streamingButton,
				 SimulcastOutput &output)
{
	QObject::connect(
		streamingButton, &QPushButton::clicked,
		[self, streamingButton]() {
			if (self->Output().IsStreaming()) {
				streamingButton->setText(
					obs_module_text("Btn.StoppingStream"));
				self->Output().StopStreaming();
			} else {
				streamingButton->setText(
					obs_module_text("Btn.StartingStream"));
				streamingButton->setDisabled(true);
				if (self->Output().StartStreaming(
					    self->StreamKey()))
					return;

				streamingButton->setText(
					obs_module_text("Btn.StartStreaming"));
				streamingButton->setDisabled(false);
			}
		});

	QObject::connect(
		&output, &SimulcastOutput::StreamStarted, self,
		[self, streamingButton]() {
			streamingButton->setText(
				obs_module_text("Btn.StopStreaming"));
			streamingButton->setDisabled(false);
		},
		Qt::QueuedConnection);

	QObject::connect(
		&output, &SimulcastOutput::StreamStopped, self,
		[self, streamingButton]() {
			streamingButton->setText(
				obs_module_text("Btn.StartStreaming"));
		},
		Qt::QueuedConnection);
}

SimulcastDockWidget::SimulcastDockWidget(QWidget * /*parent*/)
{
	QGridLayout *dockLayout = new QGridLayout(this);
	dockLayout->setAlignment(Qt::AlignmentFlag::AlignTop);

	// start all, stop all
	auto buttonContainer = new QWidget(this);
	auto buttonLayout = new QVBoxLayout();
	auto streamingButton = new QPushButton(
		obs_module_text("Btn.StartStreaming"), buttonContainer);
	buttonLayout->addWidget(streamingButton);
	buttonContainer->setLayout(buttonLayout);
	dockLayout->addWidget(buttonContainer);

	SetupSignalsAndSlots(this, streamingButton, output_);

	// load config
	LoadConfig();

	setLayout(dockLayout);

	resize(200, 400);
}

static BPtr<char> module_config_path(const char *file)
{
	return BPtr<char>(obs_module_config_path(file));
}

static OBSDataAutoRelease load_or_create_obj(obs_data_t *data, const char *name)
{
	OBSDataAutoRelease obj = obs_data_get_obj(data, name);
	if (!obj) {
		obj = obs_data_create();
		obs_data_set_obj(data, name, obj);
	}
	return obj;
}

#define JSON_CONFIG_FILE module_config_path("config.json")
#define DATA_KEY_SETTINGS_WINDOW_GEOMETRY "settings_window_geometry"
#define DATA_KEY_PROFILES "profiles"
#define DATA_KEY_STREAM_KEY "stream_key"

void SimulcastDockWidget::SaveConfig()
{
	os_mkdirs(module_config_path(""));

	// Set modified config values here
	obs_data_set_string(profile_, DATA_KEY_STREAM_KEY,
			    stream_key_.toUtf8().constData());
	obs_data_set_string(config_, DATA_KEY_SETTINGS_WINDOW_GEOMETRY,
			    settings_window_geometry_.toBase64().constData());
	// Set modified config values above

	obs_data_save_json_pretty_safe(config_, JSON_CONFIG_FILE, ".tmp",
				       ".bak");
}

void SimulcastDockWidget::LoadConfig()
{
	config_ = obs_data_create_from_json_file(JSON_CONFIG_FILE);
	if (!config_)
		config_ = obs_data_create();

	profiles_ = load_or_create_obj(config_, DATA_KEY_PROFILES);

	profile_name_->len = 0;
	dstr_cat(profile_name_, obs_frontend_get_current_profile());

	profile_ = load_or_create_obj(profiles_, profile_name_->array);

	// Set modified config values here
	stream_key_ = obs_data_get_string(profile_, DATA_KEY_STREAM_KEY);
	settings_window_geometry_ = QByteArray::fromBase64(obs_data_get_string(
		config_, DATA_KEY_SETTINGS_WINDOW_GEOMETRY));
	// Set modified config values above
}
