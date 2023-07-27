#include "simulcast-settings-window.h"
#include "simulcast-dock-widget.h"

#include <QAction>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QPushButton>
#include <QSpacerItem>
#include <QVBoxLayout>

#include <obs-frontend-api.h>
#include <obs-module.h>

void register_settings_window(SimulcastDockWidget *dock)
{
	auto action =
		static_cast<QAction *>(obs_frontend_add_tools_menu_qaction(
			obs_module_text("Settings.MenuName")));

	QMainWindow *window = (QMainWindow *)obs_frontend_get_main_window();

	obs_frontend_push_ui_translation(obs_module_get_string);
	auto settings = new SimulcastSettingsWindow(dock, window);
	obs_frontend_pop_ui_translation();

	auto cb = [settings]() {
		settings->setVisible(!settings->isVisible());
	};

	action->connect(action, &QAction::triggered, cb);
}

SimulcastSettingsWindow::SimulcastSettingsWindow(SimulcastDockWidget *dock,
						 QWidget *parent)
	: QDialog(parent), dock_(dock)
{
	auto window_layout = new QVBoxLayout(this);
	auto form_layout = new QFormLayout;

	auto stream_key_edit_layout = new QHBoxLayout;
	stream_key_edit_ = new QLineEdit;
	stream_key_show_button_ = new QPushButton;

	// Allow button box to move to bottom of window, even when the window is resized
	auto stretch_spacer = new QSpacerItem(1, 1, QSizePolicy::Minimum,
					      QSizePolicy::MinimumExpanding);

	button_box_ = new QDialogButtonBox(
		QDialogButtonBox::Apply | QDialogButtonBox::Cancel |
		QDialogButtonBox::Ok | QDialogButtonBox::Reset);

	// TODO: Disable stream key edit while stream is active (<-> OBS settings dialog)?
	stream_key_edit_layout->addWidget(stream_key_edit_);
	stream_key_edit_layout->addWidget(stream_key_show_button_);

	form_layout->addRow(obs_module_text("Settings.StreamKey"),
			    stream_key_edit_layout);
	form_layout->addItem(stretch_spacer);
	form_layout->addRow(button_box_);

	window_layout->addLayout(form_layout, 1);

	stream_key_edit_->setEchoMode(QLineEdit::Password);

	connect(stream_key_edit_, &QLineEdit::textEdited,
		[=](const QString & /* text */) { SetApplyEnabled(true); });
	connect(stream_key_show_button_, &QPushButton::clicked,
		[=](bool /*toggled*/) {
			stream_key_edit_->setEchoMode(
				stream_key_edit_->echoMode() ==
						QLineEdit::Password
					? QLineEdit::Normal
					: QLineEdit::Password);
		});
	connect(button_box_, &QDialogButtonBox::clicked,
		[=](QAbstractButton *button) { this->ButtonPressed(button); });
}

void SimulcastSettingsWindow::SetApplyEnabled(bool enabled)
{
	button_box_->button(QDialogButtonBox::Apply)->setEnabled(enabled);
}

void SimulcastSettingsWindow::ButtonPressed(QAbstractButton *button) {}
