#include "immutable-date-time.h"

ImmutableDateTime::ImmutableDateTime(QDateTime date_time)
	: date_time(date_time),
	  date_time_string(date_time.toString().toUtf8().constData())
{
}

ImmutableDateTime ImmutableDateTime::CurrentTimeUtc()
{
	return ImmutableDateTime(QDateTime::currentDateTimeUtc());
}

quint64 ImmutableDateTime::MSecsElapsed() const
{
	return date_time.msecsTo(QDateTime::currentDateTimeUtc());
}
