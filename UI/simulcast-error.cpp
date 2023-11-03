#include "simulcast-error.h"

#include <QMessageBox>
#include "obs-app.hpp"

SimulcastError SimulcastError::critical(QString error)
{
	return {Type::Critical, error};
}

SimulcastError SimulcastError::warning(QString error)
{
	return {Type::Warning, error};
}

bool SimulcastError::ShowDialog(QWidget *parent) const
{
	if (type == Type::Critical) {
		QMessageBox::critical(parent, QTStr("Output.StartStreamFailed"),
				      error, QMessageBox::StandardButton::Ok);
		return false;
	} else if (type == Type::Warning) {
		return QMessageBox::warning(
			       parent, QTStr("Output.StartStreamFailed"),
			       error + QTStr("FailedToStartStream.WarningRetryNonSimulcast"),
			       QMessageBox::StandardButton::Cancel |
				       QMessageBox::StandardButton::Retry) ==
		       QMessageBox::StandardButton::Retry;
	}
	return false;
}
