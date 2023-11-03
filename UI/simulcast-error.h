#pragma once
#include <QString>

class QWidget;

struct SimulcastError {
	static SimulcastError critical(QString error);
	static SimulcastError warning(QString error);

	bool ShowDialog(QWidget *parent) const;

	enum struct Type {
		Critical,
		Warning,
	};

	const Type type;
	const QString error;
};
