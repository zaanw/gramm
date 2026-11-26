/*
This file is part of Gramm, based on Telegram Desktop.

For license and copyright information see the repository's LEGAL file.
*/
#include "settings/sections/settings_gramm_config.h"

#include "base/weak_ptr.h"
#include "core/application.h"
#include "core/core_settings.h"
#include "core/file_utilities.h"
#include "lang/lang_keys.h"
#include "media/media_common.h"
#include "tray.h"
#include "ui/layers/generic_box.h"
#include "window/main_window.h"
#include "window/window_session_controller.h"

#include <QtCore/QFile>
#include <QtCore/QBuffer>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>
#include <QtCore/QSaveFile>
#include <QtGui/QImageReader>

namespace Settings {
namespace {

constexpr auto kRoundedIconsPref = "gramm/settings/rounded-icons";
constexpr auto kMaxConfigBytes = 16 * 1024;
constexpr auto kMaxIconBytes = 2 * 1024 * 1024;
constexpr auto kMaxLocalProfileBytes = 16 * 1024;
constexpr auto kMaxLocalGifts = 32;
constexpr auto kLocalProfilePrefix = "gramm/profile-local/";

[[nodiscard]] QString LocalProfileKey(quint64 sessionId) {
	return QString::fromLatin1(kLocalProfilePrefix)
		+ QString::number(sessionId);
}

[[nodiscard]] QString ClampLocalText(QString value, int maxLength) {
	return value.trimmed().left(maxLength);
}

void ApplyIcon(QImage image) {
	auto bytes = QByteArray();
	if (!image.isNull()) {
		auto buffer = QBuffer(&bytes);
		if (!buffer.open(QIODevice::WriteOnly)
			|| !image.save(&buffer, "PNG")) {
			return;
		}
	}
	Core::App().settings().writePref<QByteArray>(
		Core::kGrammApplicationIconPref,
		bytes);
	Window::OverrideApplicationIcon(std::move(image));
	Core::App().refreshApplicationIcon();
	Core::App().tray().updateIconCounters();
	Core::App().saveSettingsDelayed();
}

[[nodiscard]] QImage ReadIcon(const QString &path) {
	auto file = QFile(path);
	if (!file.open(QIODevice::ReadOnly) || file.size() > kMaxIconBytes) {
		return {};
	}
	auto reader = QImageReader(&file);
	const auto format = reader.format();
	if (format != "png" && format != "ico") {
		return {};
	}
	const auto size = reader.size();
	if (!size.isValid() || size.width() > 1024 || size.height() > 1024) {
		return {};
	}
	auto image = reader.read();
	return image.isNull()
		? QImage()
		: image.scaled(256, 256, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

rpl::variable<bool> &RoundedIcons() {
	static auto value = rpl::variable<bool>(
		Core::App().settings().readPref<bool>(kRoundedIconsPref, true));
	return value;
}

rpl::event_stream<> &SettingsImported() {
	static auto changes = rpl::event_stream<>();
	return changes;
}

[[nodiscard]] bool ReadConfig(const QByteArray &bytes) {
	if (bytes.isEmpty() || bytes.size() > kMaxConfigBytes) {
		return false;
	}
	const auto parsed = QJsonDocument::fromJson(bytes);
	if (!parsed.isObject()) {
		return false;
	}
	const auto values = parsed.object();
	const auto rounded = values.value(u"roundedIcons"_q);
	const auto hideTails = values.value(u"hideBubbleTails"_q);
	const auto drawerShortcut = values.value(u"drawerShortcut"_q);
	const auto musicContext = values.value(u"musicContext"_q);
	const auto repeat = values.value(u"repeatMode"_q).toInt(-1);
	const auto validRepeat = repeat == int(Media::RepeatMode::None)
		|| repeat == int(Media::RepeatMode::One)
		|| repeat == int(Media::RepeatMode::All);
	if (values.size() != 6
		|| values.value(u"version"_q).toInt(-1) != 1
		|| !rounded.isBool()
		|| !hideTails.isBool()
		|| !drawerShortcut.isBool()
		|| !musicContext.isBool()
		|| !validRepeat) {
		return false;
	}
	SetGrammRoundedIcons(rounded.toBool());
	SetGrammHideBubbleTails(hideTails.toBool());
	auto &settings = Core::App().settings();
	settings.writePref<bool>(Core::kGrammDrawerShortcutPref, drawerShortcut.toBool());
	settings.writePref<bool>(Core::kGrammMusicContextPref, musicContext.toBool());
	settings.setPlayerRepeatMode(Media::RepeatMode(repeat));
	Core::App().saveSettingsDelayed();
	SettingsImported().fire({});
	return true;
}

} // namespace

GrammLocalProfileData LoadGrammLocalProfile(quint64 sessionId) {
	GrammLocalProfileData result;
	const auto bytes = Core::App().settings().readPref<QByteArray>(
		LocalProfileKey(sessionId).toStdString());
	if (bytes.isEmpty() || bytes.size() > kMaxLocalProfileBytes) {
		return result;
	}
	const auto document = QJsonDocument::fromJson(bytes);
	if (!document.isObject()) {
		return result;
	}
	const auto object = document.object();
	if (object.value(u"ownerBadge"_q).isBool()) {
		result.ownerBadge = object.value(u"ownerBadge"_q).toBool();
	}
	if (object.value(u"number"_q).isString()) {
		result.number = ClampLocalText(
			object.value(u"number"_q).toString(),
			64);
	}
	const auto gifts = object.value(u"gifts"_q).toArray();
	for (const auto &value : gifts) {
		const auto gift = value.toObject();
		if (!gift.value(u"name"_q).isString()
			|| !gift.value(u"pattern"_q).isString()
			|| !gift.value(u"background"_q).isString()) {
			continue;
		}
		result.gifts.push_back({
			.name = ClampLocalText(gift.value(u"name"_q).toString(), 64),
			.pattern = ClampLocalText(gift.value(u"pattern"_q).toString(), 128),
			.background = ClampLocalText(
				gift.value(u"background"_q).toString(),
				128),
		});
		if (result.gifts.size() == kMaxLocalGifts) {
			break;
		}
	}
	return result;
}

void SaveGrammLocalProfile(
		quint64 sessionId,
		const GrammLocalProfileData &data) {
	QJsonArray gifts;
	for (const auto &gift : data.gifts) {
		if (gifts.size() == kMaxLocalGifts) {
			break;
		}
		gifts.append(QJsonObject{
			{ u"name"_q, ClampLocalText(gift.name, 64) },
			{ u"pattern"_q, ClampLocalText(gift.pattern, 128) },
			{ u"background"_q, ClampLocalText(gift.background, 128) },
		});
	}
	const auto bytes = QJsonDocument(QJsonObject{
		{ u"version"_q, 1 },
		{ u"ownerBadge"_q, data.ownerBadge },
		{ u"number"_q, ClampLocalText(data.number, 64) },
		{ u"gifts"_q, gifts },
	}).toJson(QJsonDocument::Compact);
	if (bytes.size() > kMaxLocalProfileBytes) {
		return;
	}
	Core::App().settings().writePref<QByteArray>(
		LocalProfileKey(sessionId).toStdString(),
		bytes);
	Core::App().saveSettingsDelayed();
}

rpl::producer<bool> GrammRoundedIcons() {
	return RoundedIcons().value();
}

rpl::producer<> GrammSettingsImported() {
	return rpl::single(rpl::empty) | rpl::then(SettingsImported().events());
}

void SetGrammRoundedIcons(bool rounded) {
	RoundedIcons() = rounded;
	Core::App().settings().writePref<bool>(kRoundedIconsPref, rounded);
	Core::App().saveSettingsDelayed();
}

bool GrammHideBubbleTails() {
	return Core::App().settings().readPref<bool>(Core::kGrammHideBubbleTailsPref);
}

void SetGrammHideBubbleTails(bool hide) {
	Core::App().settings().writePref<bool>(Core::kGrammHideBubbleTailsPref, hide);
	Core::App().saveSettingsDelayed();
}

void ImportGrammSettings(not_null<Window::SessionController*> controller) {
	const auto weak = base::make_weak(controller.get());
	FileDialog::GetOpenPath(
		Core::App().getFileDialogParent(),
		tr::lng_gramm_config_import(tr::now),
		tr::lng_gramm_config_filter(tr::now),
		[=](FileDialog::OpenResult &&result) {
			const auto strong = weak.get();
			if (!strong || (result.paths.isEmpty() && result.remoteContent.isEmpty())) {
				return;
			}
			const auto imported = [&] {
				if (!result.remoteContent.isEmpty()) {
					return ReadConfig(result.remoteContent);
				}
				auto file = QFile(result.paths.front());
				return file.open(QIODevice::ReadOnly)
					&& ReadConfig(file.read(kMaxConfigBytes + 1));
			}();
			strong->showToast(imported
				? tr::lng_gramm_config_imported(tr::now)
				: tr::lng_gramm_config_error(tr::now));
		});
}

void ExportGrammSettings(not_null<Window::SessionController*> controller) {
	const auto weak = base::make_weak(controller.get());
	FileDialog::GetWritePath(
		Core::App().getFileDialogParent(),
		tr::lng_gramm_config_export(tr::now),
		tr::lng_gramm_config_filter(tr::now),
		u"gramm-settings.json"_q,
		[=](QString &&path) {
			const auto strong = weak.get();
			if (!strong || path.isEmpty()) {
				return;
			}
			const auto data = QJsonDocument(QJsonObject{
				{ u"version"_q, 1 },
				{ u"roundedIcons"_q, RoundedIcons().current() },
				{ u"hideBubbleTails"_q, GrammHideBubbleTails() },
				{ u"drawerShortcut"_q, Core::App().settings().readPref<bool>(
					Core::kGrammDrawerShortcutPref) },
				{ u"musicContext"_q, Core::App().settings().readPref<bool>(
					Core::kGrammMusicContextPref, true) },
				{ u"repeatMode"_q, int(Core::App().settings().playerRepeatMode()) },
			}).toJson();
			auto output = QSaveFile(path);
			const auto exported = output.open(QIODevice::WriteOnly)
				&& output.write(data) == data.size()
				&& output.commit();
			strong->showToast(exported
				? tr::lng_gramm_config_exported(tr::now)
				: tr::lng_gramm_config_error(tr::now));
		});
}

void ShowGrammIconPicker(not_null<Window::SessionController*> controller) {
	const auto weak = base::make_weak(controller.get());
	controller->show(Box([=](not_null<Ui::GenericBox*> box) {
		box->setTitle(tr::lng_gramm_icon_picker());
		box->addButton(tr::lng_gramm_icon_default(), [=] {
			ApplyIcon(QImage());
			box->closeBox();
		});
		box->addButton(tr::lng_gramm_icon_round(), [=] {
			ApplyIcon(QImage(u":/gui/art/logo_256_no_margin.png"_q));
			box->closeBox();
		});
		box->addButton(tr::lng_gramm_icon_custom(), [=] {
			box->closeBox();
			FileDialog::GetOpenPath(
				Core::App().getFileDialogParent(),
				tr::lng_gramm_icon_custom(tr::now),
				tr::lng_gramm_icon_filter(tr::now),
				[=](FileDialog::OpenResult &&result) {
					const auto strong = weak.get();
					if (!strong || result.paths.isEmpty()) {
						return;
					}
					auto image = ReadIcon(result.paths.front());
					if (image.isNull()) {
						strong->showToast(tr::lng_gramm_icon_invalid(tr::now));
					} else {
						ApplyIcon(std::move(image));
					}
				});
		});
		box->addButton(tr::lng_cancel(), [=] { box->closeBox(); });
	}));
}

} // namespace Settings
