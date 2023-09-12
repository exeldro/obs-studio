#pragma once

#include <obs.hpp>
#include <optional>

struct ImmutableDateTime;

OBSDataAutoRelease constructGoLivePost(
	const ImmutableDateTime &attempt_start_time,
	const std::optional<uint64_t> &preference_maximum_bitrate,
	const std::optional<uint32_t> &preference_maximum_renditions);
