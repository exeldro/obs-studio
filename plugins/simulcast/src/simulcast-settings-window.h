#pragma once
#include <QWidget>
#include <QDialog>

#include <obs-data.h>

class SimulcastDockWidget;

class QAbstractButton;
class QCheckBox;
class QDialogButtonBox;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;

class SimulcastSettingsWindow : public QDialog {
	Q_OBJECT

public:
	SimulcastSettingsWindow(SimulcastDockWidget *widget, QWidget *parent,
				obs_data_t *settings_config);

private:
	void ButtonPressed(QAbstractButton *button);
	void SetApplyEnabled(bool enabled);

	void ResetWindow();
	void ResetSettings();

	SimulcastDockWidget *dock_;

	QLineEdit *rtmp_url_ = nullptr;

	QLineEdit *stream_key_edit_;
	QPushButton *stream_key_show_button_;
	QDialogButtonBox *button_box_;

	QCheckBox *auto_preference_maximum_bitrate_;
	QSpinBox *preference_maximum_bitrate_;

	QCheckBox *auto_preference_maximum_renditions_;
	QSpinBox *preference_maximum_renditions_;

	QCheckBox *telemetry_checkbox_;

#ifdef ENABLE_CUSTOM_TWITCH_CONFIG
	QCheckBox *use_server_config_;
	QPlainTextEdit *custom_config_;
#endif
};
