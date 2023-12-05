#pragma once

#include <obs.hpp>
#include <QFuture>
#include <QString>

#define GO_LIVE_API_PRODUCTION_URL \
	"https://ingest.twitch.tv/api/v3/GetClientConfiguration"

/** Returns either GO_LIVE_API_PRODUCTION_URL or a command line override. */
QString SimulcastAutoConfigURL();

class QWidget;

OBSDataAutoRelease DownloadGoLiveConfig(QWidget *parent, QString url,
					obs_data_t *postData);
