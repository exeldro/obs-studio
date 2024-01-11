#pragma once

#include <obs.hpp>
#include <QFuture>
#include <QString>

/** Returns either GO_LIVE_API_PRODUCTION_URL or a command line override. */
QString MultitrackVideoAutoConfigURL(obs_service_t *service);

class QWidget;

OBSDataAutoRelease DownloadGoLiveConfig(QWidget *parent, QString url,
					obs_data_t *postData);
