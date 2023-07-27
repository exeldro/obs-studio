#pragma once
#include <QWidget>
#include <QDialog>

class SimulcastDockWidget;

class QAbstractButton;
class QDialogButtonBox;
class QLineEdit;
class QPushButton;

class SimulcastSettingsWindow : public QDialog {
	Q_OBJECT

public:
	SimulcastSettingsWindow(SimulcastDockWidget *widget, QWidget *parent);

private:
	void ButtonPressed(QAbstractButton *button);
	void SetApplyEnabled(bool enabled);

	void ResetWindow();
	void ResetSettings();

	SimulcastDockWidget *dock_;

	QLineEdit *stream_key_edit_;
	QPushButton *stream_key_show_button_;
	QDialogButtonBox *button_box_;
};
