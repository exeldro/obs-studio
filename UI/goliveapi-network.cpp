#include "goliveapi-network.hpp"
#include "goliveapi-censoredjson.hpp"

#include <obs.hpp>
#include <obs-app.hpp>
#include <remote-text.hpp>
#include "simulcast-error.hpp"

#include <qstring.h>
#include <string>
#include <QMessageBox>
#include <QThreadPool>

struct ReturnValues {
	std::string encodeConfigText;
	std::string libraryError;
	bool encodeConfigDownloadedOk;
};

Qt::ConnectionType BlockingConnectionTypeFor(QObject *object);

void HandleGoLiveApiErrors(QWidget *parent, obs_data_t *config_data)
{
	OBSDataAutoRelease status = obs_data_get_obj(config_data, "status");
	if (!status) // API does not currently return status responses
		return;

	auto result = obs_data_get_string(status, "result");
	if (!result || strncmp(result, "success", 8) == 0)
		return;

	auto html_en_us = obs_data_get_string(status, "html_en_us");
	if (strncmp(result, "warning", 8) == 0) {
		OBSDataArrayAutoRelease encoder_configurations =
			obs_data_get_array(config_data,
					   "encoder_configurations");
		if (obs_data_array_count(encoder_configurations) == 0)
			throw SimulcastError::warning(html_en_us);
		else {
			bool ret = false;
			QMetaObject::invokeMethod(
				parent,
				[=] {
					QMessageBox mb(parent);
					mb.setIcon(QMessageBox::Warning);
					mb.setWindowTitle(QTStr(
						"ConfigDownload.WarningMessageTitle"));
					mb.setTextFormat(Qt::RichText);
					mb.setText(
						html_en_us +
						QTStr("FailedToStartStream.WarningRetry"));
					mb.setStandardButtons(
						QMessageBox::StandardButton::Yes |
						QMessageBox::StandardButton::No);
					return mb.exec() ==
					       QMessageBox::StandardButton::No;
				},
				BlockingConnectionTypeFor(parent), &ret);
			if (ret)
				throw SimulcastError::cancel();
		}
	} else if (strncmp(result, "error", 6) == 0) {
		throw SimulcastError::critical(html_en_us);
	}
}

OBSDataAutoRelease DownloadGoLiveConfig(QWidget *parent, QString url,
					obs_data_t *postData_)
{
	blog(LOG_INFO, "Go live POST data: %s",
	     censoredJson(postData_).toUtf8().constData());

	// andrew download code start
	OBSDataAutoRelease encodeConfigObsData;

	std::string encodeConfigText;
	std::string libraryError;

	std::vector<std::string> headers;
	headers.push_back("Content-Type: application/json");
	bool encodeConfigDownloadedOk = GetRemoteFile(
		url.toLocal8Bit(), encodeConfigText,
		libraryError, // out params
		nullptr,
		nullptr, // out params (response code and content type)
		"POST", obs_data_get_json(postData_), headers,
		nullptr, // signature
		3);      // timeout in seconds

	if (!encodeConfigDownloadedOk)
		throw SimulcastError::warning(
			QTStr("FailedToStartStream.ConfigRequestFailed")
				.arg(url, libraryError.c_str()));

#if 0
		// XXX: entirely different json parser just because it gives us errors
		// is a bit silly
		Json encodeConfigJson =
			Json::parse(encodeConfigText, libraryError);
		if (!encodeConfigJson.is_object()) {
			encodeConfigError =
				QString() + "JSON parse error: " +
				QString::fromStdString(libraryError);
		}
#endif

	encodeConfigObsData =
		obs_data_create_from_json(encodeConfigText.c_str());
	blog(LOG_INFO, "Go live Response data: %s",
	     censoredJson(encodeConfigObsData, true).toUtf8().constData());

	HandleGoLiveApiErrors(parent, encodeConfigObsData);

	return OBSData{encodeConfigObsData};
}

QString SimulcastAutoConfigURL()
{
	static const QString url = []() -> QString {
		auto args = qApp->arguments();
		for (int i = 0; i < args.length() - 1; i++) {
			if (args[i] == "--config-url") {
				return args[i + 1];
			}
		}
		return GO_LIVE_API_PRODUCTION_URL;
	}();

	blog(LOG_INFO, "Go live URL: %s", url.toUtf8().constData());
	return url;
}
