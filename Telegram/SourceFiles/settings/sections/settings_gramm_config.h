/*
This file is part of Gramm, based on Telegram Desktop.

For license and copyright information see the repository's LEGAL file.
*/
#pragma once

#include <QtCore/QString>

#include <vector>

namespace Window {
class SessionController;
} // namespace Window

namespace Settings {

struct GrammLocalGift {
	QString name;
	QString pattern;
	QString background;
};

struct GrammLocalProfileData {
	bool ownerBadge = true;
	QString number;
	std::vector<GrammLocalGift> gifts;
};

[[nodiscard]] rpl::producer<bool> GrammRoundedIcons();
[[nodiscard]] rpl::producer<> GrammSettingsImported();
void SetGrammRoundedIcons(bool rounded);
[[nodiscard]] bool GrammHideBubbleTails();
void SetGrammHideBubbleTails(bool hide);
void ImportGrammSettings(not_null<Window::SessionController*> controller);
void ExportGrammSettings(not_null<Window::SessionController*> controller);
void ShowGrammIconPicker(not_null<Window::SessionController*> controller);
[[nodiscard]] GrammLocalProfileData LoadGrammLocalProfile(quint64 sessionId);
void SaveGrammLocalProfile(quint64 sessionId, const GrammLocalProfileData &data);

} // namespace Settings
