#include "presentmon-csv-capture.hpp"

#include <QProcess>
#include <QMutex>
#include <QMutexLocker>

#include <cinttypes>

#define GAME1 "MassEffectLauncher.exe"
#define GAME2 "MassEffect3.exe"
#define GAME3 "Cyberpunk2077.exe"

#define PRESENTMON_PATH \
	"c:\\obsdev\\PresentMon\\build\\Release\\PresentMon-dev-x64.exe"

#define DISCARD_SAMPLES_BEYOND \
	144 * 60 * 2 // 144fps, one minute, times two for safety

#define PRESENTMON_SESSION_NAME "PresentMon_OBS_Twitch_Simulcast_Tech_Preview"

void PresentMonCapture_accumulator::frame(const ParsedCsvRow &row)
{
	// XXX big hack
	if (0 != strcmp(row.Application, GAME1) &&
	    0 != strcmp(row.Application, GAME2) &&
	    0 != strcmp(row.Application, GAME3))
		return;

	QMutexLocker locked(&mutex);

	// don't do this every time, it'll be slow
	// this is just a safety check so we don't allocate memory forever
	if (rows_.size() > 3 * DISCARD_SAMPLES_BEYOND)
		trimRows(locked);

	rows_.push_back(row);
}

void PresentMonCapture_accumulator::summarizeAndReset(obs_data_t *dest)
{
	double fps = -1;
	char game[PRESENTMON_APPNAME_LEN] = {0};

	QMutexLocker locked(&mutex);
#if 1
	blog(LOG_INFO,
	     "PresentMonCapture_accumulator::summarizeAndReset has %zu samples",
	     rows_.size());
#endif
	if (rows_.size() >= 2 &&
	    rows_.rbegin()->TimeInSeconds > rows_[0].TimeInSeconds) {
		trimRows(locked);
		const size_t n = rows_.size();
#if 1
		double allButFirstBetweenPresents = -rows_[0].msBetweenPresents;
		for (const auto &p : rows_)
			allButFirstBetweenPresents += p.msBetweenPresents;
		allButFirstBetweenPresents /= 1000.0;

		blog(LOG_INFO,
		     "frame timing, all but first msBetweenPresents: %f",
		     allButFirstBetweenPresents);
		blog(LOG_INFO, "frame timing, time from first to last: %f",
		     rows_[n - 1].TimeInSeconds - rows_[0].TimeInSeconds);
#endif
		fps = (n - 1) /
		      (rows_[n - 1].TimeInSeconds - rows_[0].TimeInSeconds);

		// delete all but the most recently received data point
		// XXX is this just a very convoluated rows_.erase(rows_.begin(), rows_.end()-1) ?
		*rows_.begin() = *rows_.rbegin();
		rows_.erase(rows_.begin() + 1, rows_.end());
		strncpy(game, rows_[0].Application, PRESENTMON_APPNAME_LEN - 1);
	}

	if (fps >= 0.0)
		obs_data_set_double(dest, "fps", fps);
	if (*game)
		obs_data_set_string(dest, "game", game);
}

// You need to hold the mutex before calling this
void PresentMonCapture_accumulator::trimRows(const QMutexLocker<QMutex> &)
{
	if (rows_.size() > DISCARD_SAMPLES_BEYOND) {
		rows_.erase(rows_.begin(),
			    rows_.begin() +
				    (rows_.size() - DISCARD_SAMPLES_BEYOND));
	}
}

PresentMonCapture::PresentMonCapture(QObject *parent) : QObject(parent)
{
	testCsvParser();

	process_.reset(new QProcess(this));
	state_.reset(new PresentMonCapture_state);
	accumulator_.reset(new PresentMonCapture_accumulator);

	// Log a bunch of QProcess signals
	QObject::connect(process_.get(), &QProcess::started, []() {
		blog(LOG_INFO, "QProcess::started received");
	});
	QObject::connect(
		process_.get(), &QProcess::errorOccurred,
		[](QProcess::ProcessError error) {
			blog(LOG_INFO, "%s", // FIXME: skip Qt formatting?
			     QString("QProcess::errorOccurred(error=%1) received")
				     .arg(error)
				     .toUtf8()
				     .constData());
		});
	QObject::connect(
		process_.get(), &QProcess::stateChanged,
		[](QProcess::ProcessState newState) {
			blog(LOG_INFO, "%s",
			     QString("QProcess::stateChanged(newState=%1) received")
				     .arg(newState)
				     .toUtf8()
				     .constData());
		});
	QObject::connect(
		process_.get(), &QProcess::finished,
		[](int exitCode, QProcess::ExitStatus exitStatus) {
			blog(LOG_INFO, "%s",
			     QString("QProcess::finished(exitCode=%1, exitStatus=%2) received")
				     .arg(exitCode)
				     .arg(exitStatus)
				     .toUtf8()
				     .constData());
		});

	QObject::connect(
		process_.get(), &QProcess::readyReadStandardError, [this]() {
			QByteArray data;
			while ((data = this->process_->readAllStandardError())
				       .size()) {
				blog(LOG_INFO, "StdErr: %s", data.constData());
			}
		});

	// Process the CSV as it appears on stdout
	// This will be better as a class member than a closure, because we have
	// state, and autoformat at 80 columns with size-8 tabs is really yucking
	// things up!

	QObject::connect(process_.get(), &QProcess::readyReadStandardOutput,
			 this, &PresentMonCapture::readProcessOutput_);

	// Start the proces
	QStringList args({"-output_stdout", "-stop_existing_session",
			  "-session_name", PRESENTMON_SESSION_NAME});
	process_->start(PRESENTMON_PATH, args, QIODeviceBase::ReadWrite);
}

PresentMonCapture::~PresentMonCapture()
{
	if (!process_)
		return;

	// Maybe inline functionality from
	// https://github.com/GameTechDev/PresentMon/blob/6320933cfaa373cb1702126c0449cf28b9c3431f/PresentData/TraceSession.cpp#L597-L603 instead?
	QStringList args({"-terminate_existing", "-session_name",
			  PRESENTMON_SESSION_NAME});
	auto exit_process = QProcess(this);
	exit_process.start(PRESENTMON_PATH, args);

	exit_process.waitForFinished();
	process_->waitForFinished();
}

void PresentMonCapture::summarizeAndReset(obs_data_t *dest)
{
	accumulator_->summarizeAndReset(dest);
}

void PresentMonCapture::readProcessOutput_()
{
	char buf[1024];
	char bufCsvCopy[1024];
#if 0
	if (state_->alreadyErrored_) {
		qint64 n = process_->readLine(buf, sizeof(buf));
		blog(LOG_INFO, "POST-ERROR line %d: %s", state_->lineNumber_,
		     buf);
		state_->lineNumber_++;
	}
#endif

	while (!state_->alreadyErrored_ && process_->canReadLine()) {
		qint64 n = process_->readLine(buf, sizeof(buf));
		state_->lineNumber_++; // start on line number 1

		if (n < 1 || n >= static_cast<qint64>(sizeof(buf) - 1)) {
			// XXX emit error
			state_->alreadyErrored_ = true;
		} else {
			if (buf[n - 1] == '\n') {
				//blog(LOG_INFO,
				//"REPLACING NEWLINE WITH NEW NULL");
				buf[n - 1] = '\0';
			}

			if (state_->lineNumber_ < 10) {
				blog(LOG_INFO, "Got line %" PRIu64 ": %s",
				     state_->lineNumber_, buf);
			}

			state_->v_.clear();
			memcpy(bufCsvCopy, buf, sizeof(bufCsvCopy));
			SplitCsvRow(state_->v_, bufCsvCopy);

			if (state_->lineNumber_ == 1) {
				state_->alreadyErrored_ =
					!state_->parser_.headerRow(state_->v_);
			} else {
				state_->alreadyErrored_ =
					!state_->parser_.dataRow(state_->v_,
								 &state_->row_);
				if (!state_->alreadyErrored_) {
					accumulator_->frame(state_->row_);
#if 0
					blog(LOG_INFO,
					     QString("real line received: csv line %1 Application=%3, ProcessID=%4, TimeInSeconds=%5, msBetweenPresents=%6")
						     .arg(state_->lineNumber_)
						     .arg(state_->row_
								  .Application)
						     .arg(state_->row_.ProcessID)
						     .arg(state_->row_
								  .TimeInSeconds)
						     .arg(state_->row_
								  .msBetweenPresents)
						     .toUtf8());
#endif
				}
			}
			if (state_->alreadyErrored_) {
				blog(LOG_INFO,
				     "PresentMon CSV parser failed on line %" PRIu64
				     ": %s",
				     state_->lineNumber_, buf);
			}
		}
	}
}
