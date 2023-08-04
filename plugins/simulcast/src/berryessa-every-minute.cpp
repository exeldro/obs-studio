
#include "berryessa-submitter.hpp"
#include "berryessa-every-minute.hpp"
#include "presentmon-csv-capture.hpp"

#include <QTimer>
#include <QRandomGenerator>
#include <QDateTime>

static OBSFrameCounters InitFrameCounters();

BerryessaEveryMinute::BerryessaEveryMinute(QObject *parent,
					   BerryessaSubmitter *berryessa)
	: QObject(parent),
	  berryessa_(berryessa),
	  presentmon_(this),
	  timer_(this),
	  startTime_(QDateTime::currentDateTimeUtc()),
	  frame_counters_(InitFrameCounters())
{
	obs_cpu_usage_info_.reset(os_cpu_usage_info_start());

	connect(&timer_, &QTimer::timeout, this, &BerryessaEveryMinute::fire);

	timer_.setSingleShot(true);
	quint64 msecs = QRandomGenerator::global()->generate64() % 60000;
	blog(LOG_INFO, "BerryessaEveryMinute - first invocation in %d ms",
	     (int)msecs);
	timer_.start(msecs);
}

BerryessaEveryMinute::~BerryessaEveryMinute() {}

static OBSFrameCounters InitFrameCounters()
{
	OBSFrameCounters frame_counters{};

	auto video = obs_get_video();
	frame_counters.skipped = video_output_get_skipped_frames(video);
	frame_counters.output = video_output_get_total_frames(video);

	frame_counters.rendered = obs_get_total_frames();
	frame_counters.lagged = obs_get_lagged_frames();

	return frame_counters;
}

static void AddOBSStats(os_cpu_usage_info *info,
			OBSFrameCounters &frame_counters, obs_data_t *event)
{
	obs_data_set_double(event, "obs_cpu_usage",
			    os_cpu_usage_info_query(info));
	obs_data_set_double(event, "current_obs_fps", obs_get_active_fps());
	obs_data_set_int(event, "obs_memory_usage",
			 os_get_proc_resident_size());

	obs_data_set_int(event, "total_memory", os_get_sys_total_size());
	obs_data_set_int(event, "free_memory", os_get_sys_free_size());

	// FIXME: these return 0 currently for plugin streams
	auto video = obs_get_video();
	auto skipped_frames = video_output_get_skipped_frames(video);
	auto output_frames = video_output_get_total_frames(video);

	auto rendered_frames = obs_get_total_frames();
	auto lagged_frames = obs_get_lagged_frames();

	auto maybe_update = [](uint32_t &holder, uint32_t val) {
		if (holder > val)
			holder = val;
	};
	maybe_update(frame_counters.output, output_frames);
	maybe_update(frame_counters.skipped, skipped_frames);
	maybe_update(frame_counters.rendered, rendered_frames);
	maybe_update(frame_counters.lagged, lagged_frames);

	obs_data_set_int(event, "skipped_frames",
			 skipped_frames - frame_counters.skipped);
	obs_data_set_int(event, "output_frames",
			 output_frames - frame_counters.output);

	obs_data_set_int(event, "rendered_frames",
			 rendered_frames - frame_counters.rendered);
	obs_data_set_int(event, "lagged_frames",
			 lagged_frames - frame_counters.lagged);
}

void BerryessaEveryMinute::fire()
{
	blog(LOG_INFO, "BerryessaEveryMinute::fire called");

	OBSDataAutoRelease event = obs_data_create();

	presentmon_.summarizeAndReset(event);

	AddOBSStats(obs_cpu_usage_info_.get(), frame_counters_, event);

	berryessa_->submit("ivs_obs_stream_minute", event);

	// XXX after the first firing at a random [0.000, 60.000) time, try to fire
	// every 60 seconds after that correcting for drift
	timer_.start(60000);
}
