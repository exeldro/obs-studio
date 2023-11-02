
#include "berryessa-submitter.hpp"

#include "remote-text.hpp"

#include <QTimer>

#define THROTTLE_DURATION std::chrono::seconds(5)

void SubmissionWorker::QueueEvent(OBSData event)
{
	pending_events_.emplace_back(std::move(event));

	emit PendingEvent();
}

BerryessaSubmitter::BerryessaSubmitter(QObject *parent, QString url)
	: QObject(parent),
	  url_(url),
	  submission_worker_(url)
{
	this->alwaysProperties_ = obs_data_create();

	submission_thread_.start();
	submission_worker_.moveToThread(&submission_thread_);

	connect(&submission_worker_, &SubmissionWorker::SubmissionError, this,
		&BerryessaSubmitter::SubmissionError, Qt::QueuedConnection);
	connect(this, &BerryessaSubmitter::SubmitEvent, &submission_worker_,
		&SubmissionWorker::QueueEvent);
}

BerryessaSubmitter::~BerryessaSubmitter()
{
	submission_thread_.quit();
	submission_thread_.wait();
}

void BerryessaSubmitter::submit(QString eventName, obs_data_t *properties)
{
	if (!telemetry_enabled_)
		return;

	// overlay the supplied properties over a copy of the always properties
	//   (so if there's a conflict, supplied property wins)
	OBSDataAutoRelease newProperties = obs_data_create();
	obs_data_apply(newProperties, this->alwaysProperties_);
	obs_data_apply(newProperties, properties);

	// create {Name:, Properties:} object that Berryessa expects
	OBSDataAutoRelease toplevel = obs_data_create();
	obs_data_set_string(toplevel, "event", eventName.toUtf8());
	obs_data_set_obj(toplevel, "properties", newProperties);

	emit SubmitEvent(OBSData{toplevel});
}

void SubmissionWorker::AttemptSubmission()
{
	// Berryessa documentation:
	// https://docs.google.com/document/d/1dB1fOgGQxu05ljqVVoX1jcjImzlcwcm9QKFZ2IDeuo0/edit#heading=h.yjke1ko59g7n

	if (pending_events_.empty())
		return;

	auto current_time = std::chrono::steady_clock::now();
	if (last_send_time_.has_value() &&
	    (current_time - *last_send_time_) < THROTTLE_DURATION) {
		auto diff = current_time - *last_send_time_;
		QTimer::singleShot(
			std::chrono::duration_cast<std::chrono::milliseconds>(
				diff)
				.count(),
			this, [this] { AttemptSubmission(); });
		return;
	}

	last_send_time_.emplace(std::chrono::steady_clock::now());

	QByteArray postJson;
	for (obs_data_t *it : pending_events_) {
		blog(LOG_INFO, "Berryessa: %s", obs_data_get_json(it));
		postJson += postJson.isEmpty() ? "[" : ",";
		postJson += obs_data_get_json(it);
	}
	postJson += "]";

	// base64 encoding for HTTP post
	QByteArray postEncoded("data=");
	postEncoded += postJson.toBase64();

	// http post to berryessa
	static const std::vector<std::string> headers = {
		{"User-Agent: obs-simulcast-plugin-tech-preview"},
		{"Content-Type: application/x-www-form-urlencoded; charset=UTF-8"}};

	std::string httpResponse, httpError;
	long httpResponseCode;

	bool ok = GetRemoteFile(
		url_.toUtf8(), httpResponse, httpError, // out params
		&httpResponseCode,
		nullptr, // out params (response code and content type)
		"POST", postEncoded.constData(), headers,
		nullptr, // signature
		5);      // timeout in seconds

	// XXX parse response from berryessa, check response code?

	pending_events_.clear(); // TODO: add discarded event names to error?
	if (ok)
		return;

	// log and return http error information, if any
	OBSDataAutoRelease error = obs_data_create();
	obs_data_set_string(error, "url", url_.toUtf8());
	obs_data_set_string(error, "error", httpError.c_str());
	obs_data_set_int(error, "response_code", httpResponseCode);
	blog(LOG_WARNING, "Could not submit %lld bytes to metrics backend: %s",
	     postEncoded.size(), obs_data_get_json(error));

	emit SubmissionError(OBSData{error});
}

void BerryessaSubmitter::setAlwaysString(QString propertyKey,
					 QString propertyValue)
{
	obs_data_set_string(this->alwaysProperties_, propertyKey.toUtf8(),
			    propertyValue.toUtf8());
}

void BerryessaSubmitter::unsetAlways(QString propertyKey)
{
	obs_data_unset_user_value(this->alwaysProperties_,
				  propertyKey.toUtf8());
}

void BerryessaSubmitter::enableTelemetry(bool enabled)
{
	telemetry_enabled_ = enabled;
}

void BerryessaSubmitter::SubmissionError(OBSData error)
{
	submit("ivs_obs_http_client_error", error);
}
