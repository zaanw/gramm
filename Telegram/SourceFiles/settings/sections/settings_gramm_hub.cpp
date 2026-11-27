/*
This file is part of Gramm, based on Telegram Desktop.

For license and copyright information see the repository's LEGAL file.
*/
#include "settings/sections/settings_gramm.h"

#include "base/platform/base_platform_info.h"
#include "boxes/about_box.h"
#include "boxes/compose_ai_box.h"
#include "boxes/translate_box.h"
#include "core/application.h"
#include "core/core_settings.h"
#include "core/file_utilities.h"
#include "data/data_user.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "settings/detailed_settings_button.h"
#include "settings/sections/settings_active_sessions.h"
#include "settings/sections/settings_advanced.h"
#include "settings/sections/settings_chat.h"
#include "settings/sections/settings_gramm_config.h"
#include "settings/sections/settings_main.h"
#include "settings/sections/settings_notifications.h"
#include "settings/sections/settings_privacy_security.h"
#include "settings/sections/settings_shortcuts.h"
#include "settings/settings_builder.h"
#include "settings/settings_common_session.h"
#include "ui/boxes/confirm_box.h"
#include "ui/layers/generic_box.h"
#include "ui/vertical_list.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_session_controller.h"

#include "styles/style_layers.h"
#include "styles/style_menu_icons.h"
#include "styles/style_settings.h"
#include "styles/style_info.h"

#include <algorithm>
#include <memory>

#include <QtGui/QClipboard>
#include <QtGui/QGuiApplication>

namespace Settings {
namespace {

enum class Page {
	General,
	Appearance,
	Profile,
	Privacy,
	Ai,
	About,
};

rpl::producer<QString> PageTitle(Page page) {
	switch (page) {
	case Page::General: return tr::lng_gramm_general();
	case Page::Appearance: return tr::lng_gramm_appearance();
	case Page::Profile: return tr::lng_gramm_profile();
	case Page::Privacy: return tr::lng_gramm_privacy();
	case Page::Ai: return tr::lng_gramm_ai();
	case Page::About: return tr::lng_gramm_about_title();
	}
	Unexpected("Gramm settings page.");
}

void AddAction(
		not_null<Ui::VerticalLayout*> content,
		rpl::producer<QString> title,
		Fn<void()> action) {
	const auto button = content->add(object_ptr<Ui::SettingsButton>(
		content,
		std::move(title),
		st::settingsButtonNoIcon));
	button->setClickedCallback(std::move(action));
}

void AddLink(
		not_null<Ui::VerticalLayout*> content,
		rpl::producer<QString> title,
		Fn<void(Type)> show,
		Type target) {
	AddAction(content, std::move(title), [=] { show(target); });
}

void AddLocalToggle(
		not_null<Ui::VerticalLayout*> content,
		rpl::producer<QString> title,
		const char *key,
		bool fallback) {
	const auto current = [=] {
		return Core::App().settings().readPref<bool>(key, fallback);
	};
	const auto button = content->add(object_ptr<Ui::SettingsButton>(
		content,
		std::move(title),
		st::settingsButtonNoIcon));
	button->toggleOn(GrammSettingsImported() | rpl::map(current));
	button->toggledChanges() | rpl::filter([=](bool toggled) {
		return toggled != current();
	}) | rpl::on_next([=](bool toggled) {
		Core::App().settings().writePref<bool>(key, toggled);
		Core::App().saveSettingsDelayed();
	}, button->lifetime());
}

not_null<Ui::InputField*> AddInput(
		not_null<Ui::VerticalLayout*> content,
		Ui::InputField::Mode mode,
		rpl::producer<QString> placeholder,
		QString initial = {}) {
	return content->add(
		object_ptr<Ui::InputField>(
			content,
			st::defaultInputField,
			mode,
			std::move(placeholder),
			std::move(initial)),
		st::boxRowPadding);
}

void SetupProfile(
		not_null<Ui::VerticalLayout*> content,
		not_null<Window::SessionController*> controller) {
	const auto key = std::string("gramm/profile-name/")
		+ std::to_string(controller->session().uniqueId());
	auto &settings = Core::App().settings();
	const auto initial = QString::fromUtf8(
		settings.readPref<QByteArray>(key));
	const auto preview = content->lifetime().make_state<rpl::variable<QString>>(
		initial);
	const auto localProfile = std::make_shared<GrammLocalProfileData>(
		LoadGrammLocalProfile(controller->session().uniqueId()));
	const auto ownerBadge = content->lifetime().make_state<rpl::variable<bool>>(
		localProfile->ownerBadge);
	const auto localProfilePreview = content->lifetime().make_state<
		rpl::variable<QString>>(QString());
	const auto refreshLocalProfilePreview = [=] {
		auto text = localProfile->ownerBadge
			? u"👑 G  ·  Owner Gramm"_q
			: tr::lng_gramm_owner_badge_disabled(tr::now);
		if (!localProfile->number.isEmpty()) {
			text += u"\n"_q + tr::lng_gramm_local_number_value(
				tr::now,
				lt_number,
				localProfile->number);
		}
		if (localProfile->gifts.empty()) {
			text += u"\n"_q + tr::lng_gramm_gifts_empty(tr::now);
		} else {
			text += u"\n"_q + tr::lng_gramm_gifts_count(
				tr::now,
				lt_number,
				QString::number(qulonglong(localProfile->gifts.size())));
			const auto previewCount = std::min<size_t>(
				localProfile->gifts.size(),
				3);
			for (size_t i = 0; i != previewCount; ++i) {
				const auto &gift = localProfile->gifts[i];
				text += u"\n🎁 "_q + gift.name
					+ u" · "_q + gift.pattern
					+ u" · "_q + gift.background;
			}
			if (localProfile->gifts.size() > previewCount) {
				text += u"\n…"_q;
			}
		}
		*localProfilePreview = text;
	};
	refreshLocalProfilePreview();
	Ui::AddDividerText(content, localProfilePreview->value());
	const auto badge = AddDetailedSettingsButton(
		content,
		tr::lng_gramm_owner_badge(),
		tr::lng_gramm_owner_badge_about(),
		{ .icon = &st::infoStarsCrown },
		ownerBadge->value(),
		st::grammHubRow);
	badge->setToolTip(tr::lng_gramm_owner_badge_tooltip(tr::now));
	badge->toggledChanges() | rpl::on_next([=](bool enabled) {
		localProfile->ownerBadge = enabled;
		SaveGrammLocalProfile(
			controller->session().uniqueId(),
			*localProfile);
		*ownerBadge = enabled;
		refreshLocalProfilePreview();
	}, badge->lifetime());
	Ui::AddDividerText(content, tr::lng_gramm_local_only_profile());
	const auto field = AddInput(
		content,
		Ui::InputField::Mode::SingleLine,
		tr::lng_gramm_profile_name(),
		initial);
	field->setMaxLength(64);
	Ui::AddDividerText(content, tr::lng_gramm_profile_local());
	Ui::AddDividerText(content, preview->value() | rpl::map([](QString name) {
		return name.isEmpty()
			? tr::lng_gramm_profile_default(tr::now)
			: tr::lng_gramm_profile_preview(tr::now, lt_name, name);
	}));
	const auto save = [=](const QString &name) {
		Core::App().settings().writePref<QByteArray>(key, name.toUtf8());
		Core::App().saveSettingsDelayed();
		*preview = name;
	};
	const auto number = AddInput(
		content,
		Ui::InputField::Mode::SingleLine,
		tr::lng_gramm_local_number(),
		localProfile->number);
	number->setMaxLength(64);
	const auto giftName = AddInput(
		content,
		Ui::InputField::Mode::SingleLine,
		tr::lng_gramm_gift_name());
	giftName->setMaxLength(64);
	const auto giftPattern = AddInput(
		content,
		Ui::InputField::Mode::SingleLine,
		tr::lng_gramm_gift_pattern());
	giftPattern->setMaxLength(128);
	const auto giftBackground = AddInput(
		content,
		Ui::InputField::Mode::SingleLine,
		tr::lng_gramm_gift_background());
	giftBackground->setMaxLength(128);
	AddAction(content, tr::lng_gramm_gift_add(), [=] {
		const auto name = giftName->getLastText().trimmed();
		if (name.isEmpty()) {
			giftName->showError();
			return;
		}
		localProfile->gifts.push_back({
			.name = name,
			.pattern = giftPattern->getLastText().trimmed(),
			.background = giftBackground->getLastText().trimmed(),
		});
		SaveGrammLocalProfile(
			controller->session().uniqueId(),
			*localProfile);
		giftName->setText(QString());
		giftPattern->setText(QString());
		giftBackground->setText(QString());
		refreshLocalProfilePreview();
		controller->showToast(tr::lng_gramm_gift_added(tr::now));
	});
	AddAction(content, tr::lng_gramm_profile_save_local(), [=] {
		localProfile->number = number->getLastText().trimmed();
		SaveGrammLocalProfile(
			controller->session().uniqueId(),
			*localProfile);
		refreshLocalProfilePreview();
		controller->showToast(tr::lng_gramm_profile_saved(tr::now));
	});
	AddAction(content, tr::lng_gramm_profile_copy_card(), [=] {
		const auto card = CreateGrammProfileCard(
			controller->session().userId().bare,
			*localProfile);
		if (!card.isEmpty()) {
			QGuiApplication::clipboard()->setText(card);
			controller->showToast(tr::lng_gramm_profile_card_copied(tr::now));
		}
	});
	AddAction(content, tr::lng_gramm_profile_import_card(), [=] {
		const auto imported = ImportGrammProfileCard(
			QGuiApplication::clipboard()->text());
		controller->showToast(imported
			? tr::lng_gramm_profile_card_imported(tr::now)
			: tr::lng_gramm_profile_card_invalid(tr::now));
	});
	AddAction(content, tr::lng_gramm_gifts_clear(), [=] {
		localProfile->gifts.clear();
		SaveGrammLocalProfile(
			controller->session().uniqueId(),
			*localProfile);
		refreshLocalProfilePreview();
	});
	AddAction(content, tr::lng_settings_save(), [=] {
		save(field->getLastText().trimmed());
	});
	AddAction(content, tr::lng_gramm_profile_reset(), [=] {
		field->setText(QString());
		save(QString());
		number->setText(QString());
		localProfile->number.clear();
		localProfile->gifts.clear();
		localProfile->ownerBadge = true;
		SaveGrammLocalProfile(
			controller->session().uniqueId(),
			*localProfile);
		*ownerBadge = true;
		refreshLocalProfilePreview();
	});
}

void SetupAi(
		not_null<Ui::VerticalLayout*> content,
		not_null<Window::SessionController*> controller) {
	Ui::AddDividerText(content, tr::lng_gramm_ai_manual());
	const auto field = AddInput(
		content,
		Ui::InputField::Mode::MultiLine,
		tr::lng_gramm_ai_input());
	const auto source = [=] {
		const auto text = field->getLastText().trimmed();
		if (text.isEmpty()) {
			field->showError();
		}
		return text;
	};
	AddAction(content, tr::lng_gramm_ai_translate(), [=] {
		const auto text = source();
		if (!text.isEmpty()) {
			controller->show(Box(
				Ui::TranslateTextWithTelegramBox,
				controller->session().user(),
				tr::marked(text)));
		}
	});
	AddAction(content, tr::lng_gramm_ai_edit(), [=] {
		const auto text = source();
		if (!text.isEmpty()) {
			HistoryView::Controls::ShowComposeAiBox(controller->uiShow(), {
				.session = &controller->session(),
				.text = tr::marked(text),
				.apply = crl::guard(field, [=](TextWithEntities result) {
					field->setText(result.text);
				}),
				.allowPrompt = true,
			});
		}
	});
}

void SetupPage(
		Page page,
		not_null<Ui::VerticalLayout*> content,
		not_null<Window::SessionController*> controller,
		Fn<void(Type)> show) {
	switch (page) {
	case Page::General:
		SetupLanguageButton(&controller->window(), content);
		AddLink(content, tr::lng_settings_section_notify(), show, NotificationsId());
		AddLink(content, tr::lng_settings_shortcuts(), show, ShortcutsId());
		AddLocalToggle(
			content,
			tr::lng_gramm_drawer_shortcut(),
			Core::kGrammDrawerShortcutPref,
			false);
		AddLocalToggle(
			content,
			tr::lng_gramm_music_context(),
			Core::kGrammMusicContextPref,
			true);
		if (Platform::IsWindows()) {
			AddAction(content, tr::lng_gramm_icon_picker(), [=] {
				ShowGrammIconPicker(controller);
			});
		}
		Ui::AddDivider(content);
		AddAction(content, tr::lng_gramm_register_tg(), [=] {
			controller->show(Ui::MakeConfirmBox({
				.text = tr::lng_gramm_register_tg_about(),
				.confirmed = [] { Core::RegisterTelegramUrlScheme(); },
				.confirmText = tr::lng_gramm_register_tg_confirm(),
			}));
		});
		AddAction(content, tr::lng_gramm_config_import(), [=] {
			ImportGrammSettings(controller);
		});
		AddAction(content, tr::lng_gramm_config_export(), [=] {
			ExportGrammSettings(controller);
		});
		Ui::AddDividerText(content, tr::lng_gramm_config_about());
		break;
	case Page::Appearance: {
		AddLink(content, tr::lng_settings_section_chat_settings(), show, ChatId());
		if (HasInterfaceScale()) {
			SetupInterfaceScale(&controller->window(), content);
		}
		SetupAnimations(&controller->window(), content);
		const auto button = AddDetailedSettingsButton(
			content,
			tr::lng_gramm_round_icons(),
			tr::lng_gramm_round_icons_about(),
			{ &st::menuIconPalette },
			GrammRoundedIcons(),
			st::grammHubRow);
		button->toggledChanges() | rpl::on_next([](bool value) {
			SetGrammRoundedIcons(value);
		}, button->lifetime());
		const auto tails = AddDetailedSettingsButton(
			content,
			tr::lng_gramm_hide_tails(),
			tr::lng_gramm_hide_tails_about(),
			{ &st::menuIconChatBubble },
			GrammSettingsImported() | rpl::map(GrammHideBubbleTails),
			st::grammHubRow);
		tails->toggledChanges() | rpl::on_next([](bool hidden) {
			SetGrammHideBubbleTails(hidden);
		}, tails->lifetime());
	} break;
	case Page::Profile:
		SetupProfile(content, controller);
		break;
	case Page::Privacy:
		AddLink(content, tr::lng_settings_section_privacy(), show, PrivacySecurityId());
		AddLink(content, tr::lng_settings_section_devices(), show, SessionsId());
		AddLink(content, tr::lng_settings_section_notify(), show, NotificationsId());
		break;
	case Page::Ai:
		SetupAi(content, controller);
		break;
	case Page::About:
		Ui::AddDividerText(content, tr::lng_about_version(
			lt_version,
			rpl::single(currentVersionText())));
		AddAction(content, tr::lng_gramm_about_details(), [=] {
			controller->show(Box(AboutBox));
		});
		AddAction(content, tr::lng_gramm_about_releases(), [] {
			File::OpenUrl(u"https://github.com/zaanw/gramm/releases"_q);
		});
		AddAction(content, tr::lng_gramm_about_github(), [] {
			File::OpenUrl(u"https://github.com/zaanw/gramm"_q);
		});
		break;
	}
}

template <Page Id>
class GrammPage final : public Section<GrammPage<Id>> {
public:
	GrammPage(QWidget *parent, not_null<Window::SessionController*> controller)
	: Section<GrammPage<Id>>(parent, controller) {
		const auto content = Ui::CreateChild<Ui::VerticalLayout>(this);
		SetupPage(Id, content, controller, [=](Type type) {
			this->showOther(std::move(type));
		});
		Ui::ResizeFitChild(this, content);
	}

	rpl::producer<QString> title() override {
		return PageTitle(Id);
	}
};

class GrammHub final : public Section<GrammHub> {
public:
	GrammHub(QWidget *parent, not_null<Window::SessionController*> controller)
	: Section(parent, controller) {
		const auto content = Ui::CreateChild<Ui::VerticalLayout>(this);
		Ui::AddDividerText(content, tr::lng_gramm_hub_intro());
		const auto rows = content->add(object_ptr<Ui::VerticalLayout>(content));
		GrammRoundedIcons() | rpl::on_next([=](bool rounded) {
			rows->clear();
			const auto &rowStyle = rounded
				? st::grammHubRow
				: st::grammHubRowSquare;
			const auto add = [&](rpl::producer<QString> title,
					rpl::producer<QString> description,
					const style::icon &icon,
					QRgb color,
					Type target) {
				const auto row = rows->add(object_ptr<DetailedSettingsButton>(
					rows,
					std::move(title),
					std::move(description),
					IconDescriptor{
						.icon = &icon,
						.backgroundBrush = QBrush(QColor::fromRgb(color)),
					},
					std::nullopt,
					rowStyle,
					true));
				row->setClickedCallback([=] { showOther(target); });
			};
			add(PageTitle(Page::General), tr::lng_gramm_general_desc(),
				st::menuIconSettings, 0x888888, GrammPage<Page::General>::Id());
			add(PageTitle(Page::Appearance), tr::lng_gramm_appearance_desc(),
				st::menuIconPalette, 0x9568E0, GrammPage<Page::Appearance>::Id());
			add(tr::lng_gramm_music(), tr::lng_gramm_music_desc(),
				st::menuIconSoundOn, 0xE85686, GrammMusicId());
			add(PageTitle(Page::Profile), tr::lng_gramm_profile_desc(),
				st::menuIconProfile, 0x4388DA, GrammPage<Page::Profile>::Id());
			add(PageTitle(Page::Privacy), tr::lng_gramm_privacy_desc(),
				st::menuIconLock, 0x32A480, GrammPage<Page::Privacy>::Id());
			add(PageTitle(Page::Ai), tr::lng_gramm_ai_desc(),
				st::menuIconTranslate, 0xD99B35, GrammPage<Page::Ai>::Id());
			add(tr::lng_settings_advanced(), tr::lng_gramm_advanced_desc(),
				st::menuIconManage, 0x888888, AdvancedId());
			add(PageTitle(Page::About), tr::lng_gramm_about_desc(),
				st::menuIconInfo, 0x4388DA, GrammPage<Page::About>::Id());
		}, lifetime());
		Ui::ResizeFitChild(this, content);
	}

	rpl::producer<QString> title() override {
		return tr::lng_gramm_settings();
	}
};

using SearchLabel = const tr::phrase<>*;
struct SearchPage {
	Builder::SectionMeta meta;
	std::vector<SearchLabel> options;
};

const auto kSearchPages = std::vector<SearchPage>{
	{ { GrammHub::Id(), MainId(), &tr::lng_gramm_settings,
		&st::menuIconSettings }, {} },
	{ { GrammPage<Page::General>::Id(), GrammHub::Id(),
		&tr::lng_gramm_general, &st::menuIconSettings }, {
			&tr::lng_gramm_register_tg,
			&tr::lng_gramm_drawer_shortcut,
			&tr::lng_gramm_music_context,
			&tr::lng_gramm_config_import,
			&tr::lng_gramm_config_export,
		} },
	{ { GrammPage<Page::Appearance>::Id(), GrammHub::Id(),
		&tr::lng_gramm_appearance, &st::menuIconPalette }, {
			&tr::lng_gramm_round_icons,
			&tr::lng_gramm_hide_tails,
		} },
	{ { GrammMusicId(), GrammHub::Id(),
		&tr::lng_gramm_music, &st::menuIconSoundOn }, {
			&tr::lng_gramm_queue_title,
			&tr::lng_gramm_repeat,
			&tr::lng_gramm_history_title,
		} },
	{ { GrammPage<Page::Profile>::Id(), GrammHub::Id(),
		&tr::lng_gramm_profile, &st::menuIconProfile }, {
			&tr::lng_gramm_profile_name,
			&tr::lng_gramm_profile_copy_card,
			&tr::lng_gramm_profile_import_card,
		} },
	{ { GrammPage<Page::Privacy>::Id(), GrammHub::Id(),
		&tr::lng_gramm_privacy, &st::menuIconLock }, {} },
	{ { GrammPage<Page::Ai>::Id(), GrammHub::Id(),
		&tr::lng_gramm_ai, &st::menuIconTranslate }, {
			&tr::lng_gramm_ai_translate,
			&tr::lng_gramm_ai_edit,
		} },
	{ { GrammPage<Page::About>::Id(), GrammHub::Id(),
		&tr::lng_gramm_about_title, &st::menuIconInfo }, {
			&tr::lng_gramm_about_details,
			&tr::lng_gramm_about_releases,
			&tr::lng_gramm_about_github,
		} },
};

const auto kRegisterSearch = [] {
	auto &registry = Builder::SearchRegistry::Instance();
	for (const auto &page : kSearchPages) {
		registry.add(&page.meta, [page = &page](not_null<Main::Session*>) {
			auto result = std::vector<Builder::SearchEntry>();
			if (Platform::IsWindows() && page->meta.id == GrammPage<Page::General>::Id()) {
				result.push_back({
					.title = tr::lng_gramm_icon_picker(tr::now),
					.section = page->meta.id,
					.icon = { page->meta.icon },
				});
			}
			for (const auto label : page->options) {
				result.push_back({
					.title = (*label)(tr::now),
					.section = page->meta.id,
					.icon = { page->meta.icon },
				});
			}
			return result;
		});
	}
	return true;
}();

} // namespace

Type GrammId() {
	return GrammHub::Id();
}

} // namespace Settings
