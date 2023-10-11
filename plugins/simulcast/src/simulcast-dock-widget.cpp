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
#include <QPushButton>

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
	if (self->StreamKey().trimmed().isEmpty()) {
		auto message_box = QMessageBox(
			QMessageBox::Icon::Critical,
			obs_module_text("FailedToStartStream.Title"),
			obs_module_text("FailedToStartStream.NoStreamKey"),
			QMessageBox::StandardButton::Ok, self);
		auto open_settings_button = new QPushButton(
			obs_module_text("FailedToStartStream.OpenSettings"),
			&message_box);
		message_box.addButton(open_settings_button,
				      QMessageBox::ButtonRole::AcceptRole);
		message_box.exec();

		auto clicked = message_box.clickedButton();
		if (clicked == open_settings_button)
			self->OpenSettings();

		return;
	}

	auto start_time = self->GenerateStreamAttemptStartTime();
	auto scope_name = profile_store_name(obs_get_profiler_name_store(),
					     "IVSStreamStartPressed(%s)",
					     start_time.CStr());
	ProfileScope(scope_name);

	QString url = GO_LIVE_API_URL;
	OBSDataAutoRelease custom_config_data;
#ifdef ENABLE_CUSTOM_TWITCH_CONFIG
	if (!self->UseServerConfig()) {
		const auto &custom_config = self->CustomConfig();
		if (custom_config.startsWith("http")) {
			url = custom_config;
		} else {
			custom_config_data = obs_data_create_from_json(
				self->CustomConfig().toUtf8().constData());
			if (!custom_config_data) {
				QMessageBox::critical(
					self,
					obs_module_text(
						"FailedToStartStream.Title"),
					obs_module_text(
						"FailedToStartStream.InvalidCustomConfig"));
				return;
			}
		}
	}
#endif

	streamingButton->setText(
		obs_frontend_get_locale_string("Basic.Main.Connecting"));
	streamingButton->setDisabled(true);

	OBSData postData{[&] {
		ProfileScope("constructGoLivePostData");
		return constructGoLivePost(start_time,
					   self->PreferenceMaximumBitrate(),
					   self->PreferenceMaximumRenditions());
	}()};

	DownloadGoLiveConfig(self, url, postData)
		.then(self, [=, use_custom_config = !self->UseServerConfig(),
			     custom_config_data = OBSData{custom_config_data}](
				    OBSDataAutoRelease goLiveConfig) mutable {
			auto download_time_elapsed = start_time.MSecsElapsed();

			if (!goLiveConfig && !custom_config_data) {
				streamingButton->setText(
					obs_frontend_get_locale_string(
						"Basic.Main.StartStreaming"));
				streamingButton->setDisabled(false);
				QMessageBox::critical(
					self,
					obs_module_text(
						"FailedToStartStream.Title"),
					use_custom_config
						? obs_module_text(
							  "FailedToStartStream.CustomConfigURLInvalidConfig")
						: obs_module_text(
							  "FailedToStartStream.GoLiveConfigInvalid"));
				return;
			}

			auto config_used = custom_config_data
						   ? custom_config_data
						   : OBSData{goLiveConfig};

			self->Output()
				.StartStreaming(self->DeviceId(),
						self->OBSSessionId(),
						self->RTMPURL(),
						self->StreamKey(), config_used)
				.then(self,
				      [=](bool started) {
					      if (!started) {
						      streamingButton->setText(
							      obs_frontend_get_locale_string(
								      "Basic.Main.StartStreaming"));
						      streamingButton
							      ->setDisabled(
								      false);

						      auto start_streaming_returned =
							      start_time
								      .MSecsElapsed();
						      auto event = MakeEvent_ivs_obs_stream_start_failed(
							      postData,
							      config_used,
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

					      auto event = MakeEvent_ivs_obs_stream_start(
						      postData, config_used,
						      start_time,
						      download_time_elapsed,
						      start_streaming_returned,
						      self->Output()
							      .ConnectTimeMs());

					      if (!use_custom_config) {
						      const char *configId =
							      obs_data_get_string(
								      event,
								      "config_id");
						      if (configId) {
							      // put the config_id on all events until the stream ends
							      berryessa->setAlwaysString(
								      "config_id",
								      configId);
						      }
					      }

					      berryessa->setAlwaysString(
						      "stream_attempt_start_time",
						      start_time.CStr());

					      berryessa->submit(
						      "ivs_obs_stream_start",
						      event);

					      if (!telemetry_enabled)
						      return;

					      berryessaEveryMinute->reset(new BerryessaEveryMinute(
						      self, berryessa,
						      self->Output()
							      .VideoEncoders()));
				      })
				.onFailed(self, [=](const QString &error) {
					streamingButton->setText(
						obs_frontend_get_locale_string(
							"Basic.Main.StartStreaming"));
					streamingButton->setDisabled(false);

					auto start_streaming_returned =
						start_time.MSecsElapsed();
					auto event =
						MakeEvent_ivs_obs_stream_start_failed(
							postData, config_used,
							start_time,
							download_time_elapsed,
							start_streaming_returned);
					berryessa->submit(
						"ivs_obs_stream_start_failed",
						event);

					QMessageBox::critical(
						self,
						obs_module_text(
							"FailedToStartStream.Title"),
						error);
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
	: berryessa_(this, "https://data.stats.live-video.net/")
{
	//berryessa_ = new BerryessaSubmitter(this, "http://127.0.0.1:8787/");

	obs_session_id_ = QUuid::createUuid().toString(QUuid::WithoutBraces);
	berryessa_.setAlwaysString("obs_session_id", obs_session_id_);

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
#define DATA_KEY_CUSTOM_CONFIG "custom_config"
#define DATA_KEY_DEVICE_ID "device_id"
#define DATA_KEY_PROFILES "profiles"
#define DATA_KEY_RTMP_URL "rtmp_url"
#define DATA_KEY_STREAM_KEY "stream_key"
#define DATA_KEY_PREFERENCE_MAXIMUM_BITRATE "preference_maximum_bitrate"
#define DATA_KEY_PREFERENCE_MAXIMUM_RENDITIONS "preference_maximum_renditions"
#define DATA_KEY_TELEMETRY_ENABLED "telemetry"
#define DATA_KEY_USE_TWITCH_CONFIG "use_twitch_config"
#define DATA_KEY_USE_SERVER_CONFIG "use_server_config"

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
	if (!rtmp_url_.isEmpty()) {
		obs_data_set_string(profile_, DATA_KEY_RTMP_URL,
				    rtmp_url_.toUtf8().constData());
	} else {
		obs_data_unset_user_value(profile_, DATA_KEY_RTMP_URL);
	}

	obs_data_set_string(profile_, DATA_KEY_STREAM_KEY,
			    stream_key_.toUtf8().constData());
	if (preference_maximum_bitrate_.has_value()) {
		obs_data_set_int(profile_, DATA_KEY_PREFERENCE_MAXIMUM_BITRATE,
				 preference_maximum_bitrate_.value());
	} else {
		obs_data_unset_user_value(profile_,
					  DATA_KEY_PREFERENCE_MAXIMUM_BITRATE);
	}
	if (preference_maximum_renditions_.has_value()) {
		obs_data_set_int(profile_,
				 DATA_KEY_PREFERENCE_MAXIMUM_RENDITIONS,
				 preference_maximum_renditions_.value());
	} else {
		obs_data_unset_user_value(
			profile_, DATA_KEY_PREFERENCE_MAXIMUM_RENDITIONS);
	}
	obs_data_set_bool(profile_, DATA_KEY_TELEMETRY_ENABLED,
			  telemetry_enabled_);
	obs_data_set_bool(profile_, DATA_KEY_USE_SERVER_CONFIG,
			  use_server_config_);
	obs_data_set_string(profile_, DATA_KEY_CUSTOM_CONFIG,
			    custom_config_.toUtf8().constData());
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
	obs_data_set_default_bool(profile_, DATA_KEY_USE_SERVER_CONFIG, true);

	// Migrate old config values if necessary
	if (obs_data_has_user_value(profile_, DATA_KEY_USE_TWITCH_CONFIG) &&
	    !obs_data_has_user_value(profile_, DATA_KEY_USE_SERVER_CONFIG)) {
		obs_data_set_bool(
			profile_, DATA_KEY_USE_SERVER_CONFIG,
			obs_data_get_bool(profile_,
					  DATA_KEY_USE_TWITCH_CONFIG));
	}
	// Migrate old config values above

	// Set modified config values here
	if (override_rtmp_url_ &&
	    obs_data_has_user_value(profile_, DATA_KEY_RTMP_URL)) {
		rtmp_url_ = obs_data_get_string(profile_, DATA_KEY_RTMP_URL);
	}

	stream_key_ = obs_data_get_string(profile_, DATA_KEY_STREAM_KEY);
	preference_maximum_bitrate_ =
		obs_data_has_user_value(profile_,
					DATA_KEY_PREFERENCE_MAXIMUM_BITRATE)
			? std::optional(obs_data_get_int(
				  profile_,
				  DATA_KEY_PREFERENCE_MAXIMUM_BITRATE))
			: std::nullopt;
	preference_maximum_renditions_ =
		obs_data_has_user_value(profile_,
					DATA_KEY_PREFERENCE_MAXIMUM_RENDITIONS)
			? std::optional(obs_data_get_int(
				  profile_,
				  DATA_KEY_PREFERENCE_MAXIMUM_RENDITIONS))
			: std::nullopt;
	telemetry_enabled_ =
		obs_data_get_bool(profile_, DATA_KEY_TELEMETRY_ENABLED);
	use_server_config_ =
		obs_data_get_bool(profile_, DATA_KEY_USE_SERVER_CONFIG);
	custom_config_ = obs_data_get_string(profile_, DATA_KEY_CUSTOM_CONFIG);
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

	device_id_ = obs_data_get_string(config_, DATA_KEY_DEVICE_ID);

	berryessa_.setAlwaysString("device_id", device_id_);
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

void SimulcastDockWidget::OpenSettings()
{
	if (open_settings_action_)
		open_settings_action_->trigger();
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
	QString styles;
#ifdef SIMULCAST_DOCK_STYLE_COLOR
	styles += QString::asprintf("color: %s;", SIMULCAST_DOCK_STYLE_COLOR);
#endif

#ifdef SIMULCAST_DOCK_STYLE_BACKGROUND_COLOR
	styles += QString::asprintf("background-color: %s;",
				    SIMULCAST_DOCK_STYLE_BACKGROUND_COLOR);
#endif

#ifdef SIMULCAST_DOCK_STYLE_BACKGROUND_COLOR_HEX
	styles += QString::asprintf(
		"background-color: %s;",
		"#" SIMULCAST_DOCK_STYLE_BACKGROUND_COLOR_HEX);
#endif

	if (styles.isEmpty())
		return;

	parentWidget()->setStyleSheet("QDockWidget::title {" + styles + "}");
}
