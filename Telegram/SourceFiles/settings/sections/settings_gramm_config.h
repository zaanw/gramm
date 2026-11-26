/*
This file is part of Gramm, based on Telegram Desktop.

For license and copyright information see the repository's LEGAL file.
*/
#pragma once

namespace Window {
class SessionController;
} // namespace Window

namespace Settings {

[[nodiscard]] rpl::producer<bool> GrammRoundedIcons();
[[nodiscard]] rpl::producer<> GrammSettingsImported();
void SetGrammRoundedIcons(bool rounded);
[[nodiscard]] bool GrammHideBubbleTails();
void SetGrammHideBubbleTails(bool hide);
void ImportGrammSettings(not_null<Window::SessionController*> controller);
void ExportGrammSettings(not_null<Window::SessionController*> controller);
void ShowGrammIconPicker(not_null<Window::SessionController*> controller);

} // namespace Settings
