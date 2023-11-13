#pragma once

#include <obs.hpp>
#include <optional>

struct ImmutableDateTime;

OBSDataAutoRelease
constructGoLivePost(const ImmutableDateTime &attempt_start_time,
		    const std::optional<uint64_t> &maximum_aggregate_bitrate,
		    const std::optional<uint32_t> &reserved_encoder_sessions);
