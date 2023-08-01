#pragma once

#include <obs.hpp>

struct ImmutableDateTime;

OBSDataAutoRelease
constructGoLivePost(const ImmutableDateTime &attempt_start_time);
