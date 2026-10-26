#include "vdoninja-guest-return-widget.h"

#include <obs-frontend-api.h>

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTimer>
#include <QUuid>
#include <QVBoxLayout>

#include <util/config-file.h>

#include "vdoninja-output.h"

namespace vdoninja
{

GuestReturnWidget::GuestReturnWidget(QWidget *parent) : QGroupBox(obs_module_text("GuestReturn.Title"), parent)
{
	setObjectName("VDONinjaGuestReturn");
	auto *layout = new QVBoxLayout(this);
	auto *help = new QLabel(obs_module_text("GuestReturn.Help"), this);
	help->setWordWrap(true);
	layout->addWidget(help);
	settingsPanel_ = new QWidget(this);
	auto *form = new QFormLayout(settingsPanel_);
	form->setContentsMargins(0, 0, 0, 0);
	streamId_ = new QLineEdit(settingsPanel_);
	streamId_->setObjectName("GuestReturnStreamID");
	password_ = new QLineEdit(settingsPanel_);
	password_->setEchoMode(QLineEdit::PasswordEchoOnEdit);
	room_ = new QLineEdit(settingsPanel_);
	track_ = new QSpinBox(settingsPanel_);
	track_->setObjectName("GuestReturnAudioTrack");
	track_->setRange(1, 6);
	track_->setValue(2);
	encoder_ = new QComboBox(settingsPanel_);
	encoder_->setObjectName("GuestReturnEncoder");
	encoder_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
	encoder_->setMinimumContentsLength(12);
	bitrate_ = new QSpinBox(settingsPanel_);
	bitrate_->setRange(100, 50000);
	bitrate_->setSingleStep(100);
	bitrate_->setSuffix(" kbps");
	form->addRow(obs_module_text("StreamID"), streamId_);
	form->addRow(obs_module_text("Password"), password_);
	form->addRow(obs_module_text("RoomID"), room_);
	form->addRow(obs_module_text("GuestReturn.AudioTrack"), track_);
	form->addRow(obs_module_text("GuestReturn.Encoder"), encoder_);
	form->addRow(obs_module_text("GuestReturn.Bitrate"), bitrate_);
	layout->addWidget(settingsPanel_);
	auto *buttons = new QHBoxLayout;
	start_ = new QPushButton(obs_module_text("GuestReturn.Start"), this);
	start_->setObjectName("GuestReturnStart");
	stop_ = new QPushButton(obs_module_text("GuestReturn.Stop"), this);
	stop_->setObjectName("GuestReturnStop");
	buttons->addWidget(start_);
	buttons->addWidget(stop_);
	layout->addLayout(buttons);
	copy_ = new QPushButton(obs_module_text("GuestReturn.Copy"), this);
	copy_->setObjectName("GuestReturnCopyLink");
	layout->addWidget(copy_);
	status_ = new QLabel(this);
	status_->setObjectName("GuestReturnStatus");
	status_->setWordWrap(true);
	layout->addWidget(status_);
	connect(start_, &QPushButton::clicked, this, [this] {
		saveSettings();
		publisher_.start(readSettings());
		refresh();
	});
	connect(stop_, &QPushButton::clicked, this, [this] { stop(); });
	connect(copy_, &QPushButton::clicked, this, [this] {
		const auto settings = publisher_.busy() ? publisher_.settings() : readSettings();
		QApplication::clipboard()->setText(QString::fromStdString(buildPublishUrl(settings.identity)));
	});
	for (auto *edit : {streamId_, password_, room_})
		connect(edit, &QLineEdit::editingFinished, this, [this] { saveSettings(); });
	for (auto *spin : {track_, bitrate_})
		connect(spin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this] { saveSettings(); });
	connect(encoder_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] { saveSettings(); });
	auto *timer = new QTimer(this);
	connect(timer, &QTimer::timeout, this, [this] { refresh(); });
	timer->start(1000);
}

GuestReturnSettings GuestReturnWidget::readSettings() const
{
	GuestReturnSettings settings;
	settings.identity = {streamId_->text().trimmed().toStdString(),
	                     password_->text().toStdString(),
	                     room_->text().trimmed().toStdString(),
	                     {},
	                     {}};
	settings.audioTrack = track_->value();
	settings.videoBitrate = bitrate_->value();
	settings.videoEncoder = encoder_->currentData().toString().toStdString();
	return settings;
}

void GuestReturnWidget::loadSettings()
{
	loading_ = true;
	publisher_.clearError();
	auto *config = obs_frontend_get_profile_config();
	if (!config) {
		loading_ = false;
		return;
	}
	const auto get = [config](const char *key) {
		const char *value = config_get_string(config, "VDONinjaGuestReturn", key);
		return QString::fromUtf8(value ? value : "");
	};
	streamId_->setText(get("StreamID"));
	if (streamId_->text().isEmpty())
		streamId_->setText("return" + QUuid::createUuid().toString(QUuid::Id128).left(12));
	password_->setText(get("Password"));
	room_->setText(get("RoomID"));
	const auto track = config_get_int(config, "VDONinjaGuestReturn", "AudioTrack");
	track_->setValue(track >= 1 && track <= 6 ? static_cast<int>(track) : 2);
	const auto bitrate = config_get_int(config, "VDONinjaGuestReturn", "VideoBitrate");
	bitrate_->setValue(bitrate >= 100 && bitrate <= 50000 ? static_cast<int>(bitrate) : 2500);
	encoder_->clear();
	for (const auto &encoder : GuestReturnOutput::videoEncoders())
		encoder_->addItem(QString::fromStdString(encoder.name), QString::fromStdString(encoder.id));
	const auto savedEncoder = get("VideoEncoder");
	if (!savedEncoder.isEmpty()) {
		int index = encoder_->findData(savedEncoder);
		if (index < 0) {
			encoder_->addItem(QString::fromUtf8(obs_module_text("GuestReturn.MissingEncoder")).arg(savedEncoder),
			                  savedEncoder);
			index = encoder_->count() - 1;
		}
		encoder_->setCurrentIndex(index);
	}
	loading_ = false;
	saveSettings();
	refresh();
}

void GuestReturnWidget::saveSettings()
{
	if (loading_)
		return;
	auto *config = obs_frontend_get_profile_config();
	if (!config)
		return;
	const auto settings = readSettings();
	config_set_string(config, "VDONinjaGuestReturn", "StreamID", settings.identity.streamId.c_str());
	config_set_string(config, "VDONinjaGuestReturn", "Password", settings.identity.password.c_str());
	config_set_string(config, "VDONinjaGuestReturn", "RoomID", settings.identity.roomId.c_str());
	config_set_string(config, "VDONinjaGuestReturn", "VideoEncoder", settings.videoEncoder.c_str());
	config_set_int(config, "VDONinjaGuestReturn", "AudioTrack", settings.audioTrack);
	config_set_int(config, "VDONinjaGuestReturn", "VideoBitrate", settings.videoBitrate);
	config_save(config);
}

void GuestReturnWidget::stop()
{
	publisher_.stop();
	refresh();
}

void GuestReturnWidget::refresh()
{
	const bool busy = publisher_.busy();
	settingsPanel_->setEnabled(!busy);
	start_->setEnabled(!busy);
	stop_->setEnabled(busy);
	copy_->setEnabled(busy || !streamId_->text().trimmed().isEmpty());
	const auto error = publisher_.error();
	if (!error.empty()) {
		status_->setText(obs_module_text(error.c_str()));
	} else if (busy) {
		auto *vdo = static_cast<VDONinjaOutput *>(obs_obj_get_data(publisher_.output()));
		status_->setText(vdo && vdo->isConnected() ? QString::fromUtf8(obs_module_text("GuestReturn.Live"))
		                                                 .arg(publisher_.settings().audioTrack)
		                                                 .arg(vdo->getViewerCount())
		                                           : QString::fromUtf8(obs_module_text("GuestReturn.Connecting")));
	} else {
		status_->setText(obs_module_text("GuestReturn.Idle"));
	}
	// A failed asynchronous start can still own initialized encoders and workers.
	if (!busy && publisher_.output())
		publisher_.stop();
}

} // namespace vdoninja
