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
#include <QtCore/QJsonObject>
#include <QtCore/QSaveFile>
#include <QtGui/QImageReader>

namespace Settings {
namespace {

constexpr auto kRoundedIconsPref = "gramm/settings/rounded-icons";
constexpr auto kMaxConfigBytes = 16 * 1024;
constexpr auto kMaxIconBytes = 2 * 1024 * 1024;

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
