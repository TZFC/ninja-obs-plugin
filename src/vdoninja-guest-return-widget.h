#pragma once

#include <QGroupBox>

#include "vdoninja-guest-return.h"

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;

namespace vdoninja
{

class GuestReturnWidget : public QGroupBox
{
public:
	explicit GuestReturnWidget(QWidget *parent);
	void loadSettings();
	void stop();

private:
	GuestReturnSettings readSettings() const;
	void saveSettings();
	void refresh();
	GuestReturnOutput publisher_;
	QWidget *settingsPanel_;
	QLineEdit *streamId_;
	QLineEdit *password_;
	QLineEdit *room_;
	QComboBox *encoder_;
	QSpinBox *track_;
	QSpinBox *bitrate_;
	QPushButton *start_;
	QPushButton *stop_;
	QPushButton *copy_;
	QLabel *status_;
	bool loading_ = true;
};

} // namespace vdoninja
