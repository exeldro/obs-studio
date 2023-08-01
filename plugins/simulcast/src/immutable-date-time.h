#pragma once

#include <string>
#include <QDateTime>

struct ImmutableDateTime {
	ImmutableDateTime(QDateTime date_time);

	static ImmutableDateTime CurrentTimeUtc();

	const char *CStr() const { return date_time_string.c_str(); }
	quint64 MSecsElapsed() const;

	const QDateTime date_time;
	const std::string date_time_string;
};
