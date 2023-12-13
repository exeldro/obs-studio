#pragma once

#include <functional>
#include <QFuture>

template<typename T> struct FutureHolder {
	std::function<void()> cancelAll;
	QFuture<T> future;
};
