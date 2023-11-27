#pragma once
#include <QString>

class QWidget;

struct SimulcastError {
	static SimulcastError critical(QString error);
	static SimulcastError warning(QString error);
	static SimulcastError cancel();

	bool ShowDialog(QWidget *parent) const;

	enum struct Type {
		Critical,
		Warning,
		Cancel,
	};

	const Type type;
	const QString error;
};
