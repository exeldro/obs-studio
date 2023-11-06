#pragma once

#include <obs.hpp>
#include <QFuture>
#include <QString>

#define GO_LIVE_API_URL "https://ingest.twitch.tv/api/v3/GetClientConfiguration"

class QWidget;

OBSDataAutoRelease DownloadGoLiveConfig(QWidget *parent, QString url,
					obs_data_t *postData);
