#include "goliveapi-network.hpp"

#include <obs.hpp>
#include <obs-app.hpp>
#include <remote-text.hpp>

#include <qstring.h>
#include <string>
#include <QMessageBox>
#include <QThreadPool>

struct ReturnValues {
	std::string encodeConfigText;
	std::string libraryError;
	bool encodeConfigDownloadedOk;
};

OBSDataAutoRelease DownloadGoLiveConfig(QWidget *parent, QString url,
					obs_data_t *postData_)
{
	blog(LOG_INFO, "Go live POST data: %s", obs_data_get_json(postData_));

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

	QString encodeConfigError;

	if (!encodeConfigDownloadedOk) {
		encodeConfigError =
			QString() + "Could not fetch config from " + url +
			"\n\nHTTP error: " +
			QString::fromStdString(libraryError) +
			"\n\nDo you want to stream anyway? You'll only stream a single quality option.";
	} else {
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
		     obs_data_get_json(encodeConfigObsData));
	}

	if (!encodeConfigError.isEmpty()) {
		int carryOn = QMessageBox::warning(
			parent,
			QString::asprintf(Str("ConfigDownloadError.Title"),
					  SIMULCAST_DOCK_TITLE),
			encodeConfigError, QMessageBox::Yes, QMessageBox::No);

		if (carryOn != QMessageBox::Yes)
			return nullptr; //false;

		encodeConfigObsData = nullptr;
	}

	blog(LOG_INFO, "Go Live Config data: %s", encodeConfigText.c_str());

	return OBSData{encodeConfigObsData};
}
