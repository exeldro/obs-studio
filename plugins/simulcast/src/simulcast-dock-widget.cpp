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
#include <QUuid>

#include <obs.h>
#include <obs-frontend-api.h>
#include <obs-module.h>
#include <util/config-file.h>
#include <util/platform.h>
#include <util/profiler.hpp>
#include <util/util.hpp>

#include <algorithm>
#include <vector>

#include "ivs-events.h"
#include "goliveapi-network.hpp"
#include "goliveapi-postdata.hpp"
#include "berryessa-submitter.hpp"
#include "presentmon-csv-capture.hpp"

#define ConfigSection "simulcast"

#define GO_LIVE_API_URL "https://ingest.twitch.tv/api/v3/GetClientConfiguration"

static void
handle_stream_start(SimulcastDockWidget *self, QPushButton *streamingButton,
		    BerryessaSubmitter *berryessa,
		    std::unique_ptr<BerryessaEveryMinute> *berryessaEveryMinute,
		    bool telemetry_enabled)
{
	auto start_time = self->GenerateStreamAttemptStartTime();
	auto scope_name = profile_store_name(obs_get_profiler_name_store(),
					     "IVSStreamStartPressed(%s)",
					     start_time.CStr());
	ProfileScope(scope_name);

	streamingButton->setText(
		obs_frontend_get_locale_string("Basic.Main.Connecting"));
	streamingButton->setDisabled(true);

	OBSData postData{[&] {
		ProfileScope("constructGoLivePostData");
		return constructGoLivePost(start_time);
	}()};
	DownloadGoLiveConfig(self, GO_LIVE_API_URL, postData).then(self, [=](OBSDataAutoRelease goLiveConfig) mutable {
		auto download_time_elapsed = start_time.MSecsElapsed();

		self->Output()
			.StartStreaming(self->StreamKey(), goLiveConfig)
			.then(self,
			      [=, goLiveConfig =
					  OBSData{goLiveConfig}](bool started) {
				      if (!started) {
					      streamingButton->setText(
						      obs_frontend_get_locale_string(
							      "Basic.Main.StartStreaming"));
					      streamingButton->setDisabled(
						      false);

					      auto start_streaming_returned =
						      start_time.MSecsElapsed();
					      auto event = MakeEvent_ivs_obs_stream_start_failed(
						      postData, goLiveConfig,
						      start_time,
						      download_time_elapsed,
						      start_streaming_returned);
					      berryessa->submit(
						      "ivs_obs_stream_start_failed",
						      event);
					      return;
				      }

				      auto start_streaming_returned =
					      start_time.MSecsElapsed();

				      auto event =
					      MakeEvent_ivs_obs_stream_start(
						      postData, goLiveConfig,
						      start_time,
						      download_time_elapsed,
						      start_streaming_returned,
						      self->Output()
							      .ConnectTimeMs());
				      const char *configId =
					      obs_data_get_string(event,
								  "config_id");
				      if (configId) {
					      // put the config_id on all events until the stream ends
					      berryessa->setAlwaysString(
						      "config_id", configId);
				      }
				      berryessa->setAlwaysString(
					      "stream_attempt_start_time",
					      start_time.CStr());

				      berryessa->submit("ivs_obs_stream_start",
							event);

				      if (!telemetry_enabled)
					      return;

				      berryessaEveryMinute->reset(
					      new BerryessaEveryMinute(
						      self, berryessa,
						      self->Output()
							      .VideoEncoders()));
			      })
			.onFailed(self, [=,
					 goLiveConfig = OBSData{goLiveConfig}](
						const QString &error) {
				streamingButton->setText(
					obs_frontend_get_locale_string(
						"Basic.Main.StartStreaming"));
				streamingButton->setDisabled(false);

				auto start_streaming_returned =
					start_time.MSecsElapsed();
				auto event =
					MakeEvent_ivs_obs_stream_start_failed(
						postData, goLiveConfig,
						start_time,
						download_time_elapsed,
						start_streaming_returned);
				berryessa->submit("ivs_obs_stream_start_failed",
						  event);

				QMessageBox::critical(
					self, "Failed to start stream", error);
			});
	});
}

static void SetupSignalsAndSlots(
	SimulcastDockWidget *self, QPushButton *streamingButton,
	SimulcastOutput &output, BerryessaSubmitter &berryessa,
	std::unique_ptr<BerryessaEveryMinute> &berryessaEveryMinute,
	bool &telemetry_enabled)
{
	QObject::connect(
		streamingButton, &QPushButton::clicked,
		[self, streamingButton, berryessa = &berryessa,
		 berryessaEveryMinute = &berryessaEveryMinute,
		 &telemetry_enabled]() {
			if (self->Output().IsStreaming()) {
				streamingButton->setText(
					obs_frontend_get_locale_string(
						"Basic.Main.StoppingStreaming"));
				self->Output().StopStreaming();

				berryessa->submit(
					"ivs_obs_stream_stop",
					MakeEvent_ivs_obs_stream_stop());

				berryessaEveryMinute->reset(nullptr);

				berryessa->unsetAlways("config_id");
			} else {
				handle_stream_start(self, streamingButton,
						    berryessa,
						    berryessaEveryMinute,
						    telemetry_enabled);
			}
		});

	QObject::connect(
		&output, &SimulcastOutput::StreamStarted, self,
		[self, streamingButton, berryessa = &berryessa]() {
			streamingButton->setText(obs_frontend_get_locale_string(
				"Basic.Main.StopStreaming"));
			streamingButton->setDisabled(false);

			auto &start_time = self->StreamAttemptStartTime();
			if (!start_time.has_value())
				return;

			auto event = MakeEvent_ivs_obs_stream_started(
				start_time->MSecsElapsed());
			berryessa->submit("ivs_obs_stream_started", event);
		},
		Qt::QueuedConnection);

	QObject::connect(
		&output, &SimulcastOutput::StreamStopped, self,
		[self, streamingButton]() {
			streamingButton->setText(obs_frontend_get_locale_string(
				"Basic.Main.StartStreaming"));
			streamingButton->setDisabled(false);
		},
		Qt::QueuedConnection);
}

SimulcastDockWidget::SimulcastDockWidget(QWidget * /*parent*/)
	: berryessa_(this, "https://data-staging.stats.live-video.net/")
{
	//berryessa_ = new BerryessaSubmitter(this, "http://127.0.0.1:8787/");

	berryessa_.setAlwaysString(
		"obs_session_id",
		QUuid::createUuid().toString(QUuid::WithoutBraces));

	QGridLayout *dockLayout = new QGridLayout(this);
	dockLayout->setAlignment(Qt::AlignmentFlag::AlignTop);

	// start all, stop all
	auto buttonContainer = new QWidget(this);
	auto buttonLayout = new QVBoxLayout();
	auto streamingButton = new QPushButton(
		obs_frontend_get_locale_string("Basic.Main.StartStreaming"),
		buttonContainer);
	buttonLayout->addWidget(streamingButton);
	buttonContainer->setLayout(buttonLayout);
	dockLayout->addWidget(buttonContainer);

	SetupSignalsAndSlots(this, streamingButton, output_, berryessa_,
			     berryessaEveryMinute_, telemetry_enabled_);

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
#define DATA_KEY_MAKE_DOCK_VISIBLE_PROMPT "make_dock_visible_prompt"
#define DATA_KEY_DEVICE_ID "device_id"
#define DATA_KEY_PROFILES "profiles"
#define DATA_KEY_STREAM_KEY "stream_key"
#define DATA_KEY_TELEMETRY_ENABLED "telemetry"

static void write_config(obs_data_t *config)
{
	obs_data_save_json_pretty_safe(config, JSON_CONFIG_FILE, ".tmp",
				       ".bak");
}

void SimulcastDockWidget::SaveConfig()
{
	berryessa_.enableTelemetry(telemetry_enabled_);

	os_mkdirs(module_config_path(""));

	// Set modified config values here
	obs_data_set_string(profile_, DATA_KEY_STREAM_KEY,
			    stream_key_.toUtf8().constData());
	obs_data_set_bool(profile_, DATA_KEY_TELEMETRY_ENABLED,
			  telemetry_enabled_);
	obs_data_set_string(config_, DATA_KEY_SETTINGS_WINDOW_GEOMETRY,
			    settings_window_geometry_.toBase64().constData());
	if (make_dock_visible_prompt_.has_value()) {
		obs_data_set_string(config_, DATA_KEY_MAKE_DOCK_VISIBLE_PROMPT,
				    make_dock_visible_prompt_
					    ->toString(Qt::ISODate)
					    .toUtf8()
					    .constData());
	}
	// Set modified config values above

	write_config(config_);
}

void SimulcastDockWidget::LoadConfig()
{
	config_ = obs_data_create_from_json_file(JSON_CONFIG_FILE);
	if (!config_)
		config_ = obs_data_create();

	profiles_ = load_or_create_obj(config_, DATA_KEY_PROFILES);

	dstr_free(profile_name_);
	dstr_init_move_array(profile_name_, obs_frontend_get_current_profile());

	profile_ = load_or_create_obj(profiles_, profile_name_->array);

	obs_data_set_default_bool(profile_, DATA_KEY_TELEMETRY_ENABLED, true);

	// Set modified config values here
	stream_key_ = obs_data_get_string(profile_, DATA_KEY_STREAM_KEY);
	telemetry_enabled_ =
		obs_data_get_bool(profile_, DATA_KEY_TELEMETRY_ENABLED);
	settings_window_geometry_ = QByteArray::fromBase64(obs_data_get_string(
		config_, DATA_KEY_SETTINGS_WINDOW_GEOMETRY));
	if (obs_data_has_user_value(config_,
				    DATA_KEY_MAKE_DOCK_VISIBLE_PROMPT)) {
		make_dock_visible_prompt_ = QDateTime::fromString(
			obs_data_get_string(config_,
					    DATA_KEY_MAKE_DOCK_VISIBLE_PROMPT),
			Qt::ISODate);
	}
	// Set modified config values above

	// ==================== device id ====================
	if (!obs_data_has_user_value(config_, DATA_KEY_DEVICE_ID)) {
		auto new_device_id =
			QUuid::createUuid().toString(QUuid::WithoutBraces);

		obs_data_set_string(config_, DATA_KEY_DEVICE_ID,
				    new_device_id.toUtf8().constData());
		write_config(config_);
	}

	berryessa_.setAlwaysString(
		"device_id", obs_data_get_string(config_, DATA_KEY_DEVICE_ID));
	// ==================== device id ====================

	emit ProfileChanged();
}

void SimulcastDockWidget::ProfileRenamed()
{
	DStr new_profile_name;
	dstr_cat(new_profile_name, obs_frontend_get_current_profile());

	obs_data_erase(profiles_, profile_name_->array);
	obs_data_set_obj(profiles_, new_profile_name->array, profile_);

	profile_name_ = std::move(new_profile_name);

	write_config(config_);
}

void SimulcastDockWidget::PruneDeletedProfiles()
{
	std::vector<const char *> profile_names;
	for (auto profile_item = obs_data_first(profiles_); profile_item;
	     obs_data_item_next(&profile_item)) {
		profile_names.push_back(obs_data_item_get_name(profile_item));
	}

	BPtr<char *> profiles = obs_frontend_get_profiles();
	for (size_t i = 0; profiles[i]; i++) {
		auto it = std::find_if(
			profile_names.begin(), profile_names.end(),
			[&](const char *name) {
				return qstrcmp(name, profiles[i]) == 0;
			});
		if (it != profile_names.end())
			profile_names.erase(it);
	}

	for (auto &profile : profile_names) {
		obs_data_erase(profiles_, profile);
	}

	write_config(config_);
}

void SimulcastDockWidget::CheckPromptToMakeDockVisible()
{
	if (isVisible())
		return;

	if (make_dock_visible_prompt_.has_value()) {
		if (*make_dock_visible_prompt_ >
		    QDateTime::currentDateTimeUtc())
			return;
	}

	auto prompt = QMessageBox(QMessageBox::Icon::Question,
				  obs_module_text("VisibilityPrompt.Title"),
				  obs_module_text("VisibilityPrompt.Text"),
				  QMessageBox::Yes | QMessageBox::No, this);
	auto remind_button = prompt.addButton(
		obs_module_text("VisibilityPrompt.RemindOneWeek"),
		QMessageBox::ButtonRole::DestructiveRole);
	prompt.exec();

	auto button = prompt.clickedButton();
	if (button == remind_button) {
		make_dock_visible_prompt_ =
			QDateTime::currentDateTimeUtc().addDays(7);
		SaveConfig();
	} else if (button == prompt.button(QMessageBox::StandardButton::Yes)) {
		// this widget is wrapped in an OBSDock widget currently,
		// so we need to call show on the parent widget
		auto parent = parentWidget();
		if (parent)
			parent->show();
	}
}

const ImmutableDateTime &SimulcastDockWidget::GenerateStreamAttemptStartTime()
{
	stream_attempt_start_time_.emplace(ImmutableDateTime::CurrentTimeUtc());
	return *stream_attempt_start_time_;
}

const std::optional<ImmutableDateTime> &
SimulcastDockWidget::StreamAttemptStartTime() const
{
	return stream_attempt_start_time_;
}

void SimulcastDockWidget::SetParentStyleSheet()
{
	parentWidget()->setStyleSheet(
		"QDockWidget::title { background-color: #644186; color: white; }");
}
