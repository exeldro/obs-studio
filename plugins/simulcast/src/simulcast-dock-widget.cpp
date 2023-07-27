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
#include <obs-module.h>
#include <util/config-file.h>
#include <obs-frontend-api.h>

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
				if (self->Output().StartStreaming())
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

void SimulcastDockWidget::SaveConfig() {}

void SimulcastDockWidget::LoadConfig() {}
