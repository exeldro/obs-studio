#include "simulcast-error.hpp"

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

SimulcastError SimulcastError::cancel()
{
	return {Type::Cancel, {}};
}

bool SimulcastError::ShowDialog(QWidget *parent) const
{
	#if 0
	if (type == Type::Critical) {
		QMessageBox::critical(parent, QTStr("Output.StartStreamFailed"),
				      QString("<html>") + error,
				      QMessageBox::StandardButton::Ok);
		return false;
	} else if (type == Type::Warning) {
		return QMessageBox::warning(
			       parent, QTStr("Output.StartStreamFailed"),
			       QString("<html>") + error +
				       QTStr("FailedToStartStream.WarningRetryNonSimulcast"),
			       QMessageBox::StandardButton::Yes |
				       QMessageBox::StandardButton::No) ==
		       QMessageBox::StandardButton::Yes;
	}
	#endif
	QMessageBox mb(parent);
	mb.setTextFormat(Qt::RichText);
	mb.setWindowTitle(QTStr("Output.StartStreamFailed"));

	if (type == Type::Warning) {
		mb.setText(
			error +
			QTStr("FailedToStartStream.WarningRetryNonSimulcast"));
		mb.setIcon(QMessageBox::Warning);
		mb.setStandardButtons(QMessageBox::StandardButton::Yes |
				      QMessageBox::StandardButton::No);
		return mb.exec() == QMessageBox::StandardButton::Yes;
	} else if (type == Type::Critical) {
		mb.setText(error);
		mb.setIcon(QMessageBox::Critical);
		mb.setStandardButtons(QMessageBox::StandardButton::Ok); // cannot continue
		mb.exec();
	}

	return false;
}
