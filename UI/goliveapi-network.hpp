#pragma once

#include <obs.hpp>
#include <QFuture>
#include <QString>

class QWidget;

OBSDataAutoRelease DownloadGoLiveConfig(QWidget *parent, QString url,
					obs_data_t *postData);
