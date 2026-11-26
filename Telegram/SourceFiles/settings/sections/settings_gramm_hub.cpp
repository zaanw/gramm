/*
This file is part of Gramm, based on Telegram Desktop.

For license and copyright information see the repository's LEGAL file.
*/
#include "settings/sections/settings_gramm.h"

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
#include "settings/sections/settings_main.h"
#include "settings/sections/settings_notifications.h"
#include "settings/sections/settings_privacy_security.h"
#include "settings/sections/settings_shortcuts.h"
#include "settings/settings_common_session.h"
#include "ui/layers/generic_box.h"
#include "ui/vertical_list.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_session_controller.h"

#include "styles/style_layers.h"
#include "styles/style_menu_icons.h"
#include "styles/style_settings.h"

namespace Settings {
namespace {

constexpr auto kRoundedIconsPref = "gramm/settings/rounded-icons";

enum class Page {
	General,
	Appearance,
	Profile,
	Privacy,
	Ai,
	About,
};

rpl::variable<bool> &RoundedIcons() {
	static auto value = rpl::variable<bool>(
		Core::App().settings().readPref<bool>(kRoundedIconsPref, true));
	return value;
}

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
	AddAction(content, tr::lng_settings_save(), [=] {
		save(field->getLastText().trimmed());
	});
	AddAction(content, tr::lng_gramm_profile_reset(), [=] {
		field->setText(QString());
		save(QString());
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
		break;
	case Page::Appearance: {
		AddLink(content, tr::lng_settings_section_chat_settings(), show, ChatId());
		if (HasInterfaceScale()) {
			SetupInterfaceScale(&controller->window(), content);
		}
		SetupAnimations(&controller->window(), content);
		auto &rounded = RoundedIcons();
		const auto button = AddDetailedSettingsButton(
			content,
			tr::lng_gramm_round_icons(),
			tr::lng_gramm_round_icons_about(),
			{ &st::menuIconPalette },
			rounded.value(),
			st::grammHubRow);
		button->toggledChanges() | rpl::on_next([](bool value) {
			RoundedIcons() = value;
			Core::App().settings().writePref<bool>(kRoundedIconsPref, value);
			Core::App().saveSettingsDelayed();
		}, button->lifetime());
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
		RoundedIcons().value() | rpl::on_next([=](bool rounded) {
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

} // namespace

Type GrammId() {
	return GrammHub::Id();
}

} // namespace Settings
