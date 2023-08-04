#include "qt-helpers.h"
#include <QPromise>

QFuture<void> CreateFuture()
{
	QPromise<void> promise;
	auto future = promise.future();
	promise.start();
	promise.finish();
	return future;
}
