/*
This file is part of Gramm, based on Telegram Desktop.

For license and copyright information see the repository's LEGAL file.
*/
#include "settings/sections/settings_gramm.h"

#include "core/application.h"
#include "core/core_settings.h"
#include "lang/lang_keys.h"
#include "media/media_common.h"
#include "media/player/media_player_instance.h"
#include "media/player/media_player_music_queue.h"
#include "settings/settings_common_session.h"
#include "ui/layers/generic_box.h"
#include "ui/text/format_song_document_name.h"
#include "ui/ui_utility.h"
#include "ui/vertical_list.h"
#include "ui/widgets/buttons.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_session_controller.h"

#include "styles/style_settings.h"

namespace Settings {
namespace {

using Media::Player::MusicQueueTrack;
constexpr auto kSong = AudioMsgId::Type::Song;

Ui::SettingsButton *AddQueueButton(
		not_null<Ui::VerticalLayout*> layout,
		rpl::producer<QString> text,
		Fn<void()> callback) {
	const auto button = layout->add(object_ptr<Ui::SettingsButton>(
		layout,
		std::move(text),
		st::settingsButtonNoIcon));
	button->setClickedCallback(std::move(callback));
	return button;
}

void ShowQueueTrack(
		not_null<Window::SessionController*> controller,
		MusicQueueTrack track,
		QString title) {
	controller->show(Box([=](not_null<Ui::GenericBox*> box) {
		box->setTitle(rpl::single(title));
		const auto player = Media::Player::instance();
		const auto session = &controller->session();
		const auto &tracks = player->musicQueue(session).tracks();
		const auto i = ranges::find(tracks, track);
		if (i != end(tracks)) {
			const auto inner = box->verticalLayout();
			AddQueueButton(inner, tr::lng_gramm_music_play(), [=] {
				const auto audio = player->musicQueue(session).resolve(track);
				box->closeBox();
				if (audio) {
					player->play(audio);
				}
			});
			if (i != begin(tracks)) {
				AddQueueButton(inner, tr::lng_gramm_music_up(), [=] {
					box->closeBox();
					player->musicQueue(session).move(track, -1);
				});
			}
			if (i + 1 != end(tracks)) {
				AddQueueButton(inner, tr::lng_gramm_music_down(), [=] {
					box->closeBox();
					player->musicQueue(session).move(track, 1);
				});
			}
			AddQueueButton(inner, tr::lng_gramm_music_remove(), [=] {
				box->closeBox();
				player->musicQueue(session).remove(track);
			});
		}
		box->addButton(tr::lng_close(), [=] { box->closeBox(); });
	}));
}

class MusicPlus final : public Section<MusicPlus> {
public:
	MusicPlus(QWidget *parent, not_null<Window::SessionController*> controller);

	[[nodiscard]] rpl::producer<QString> title() override;
};

MusicPlus::MusicPlus(
		QWidget *parent,
		not_null<Window::SessionController*> controller)
: Section(parent, controller) {
	const auto content = Ui::CreateChild<Ui::VerticalLayout>(this);
	Ui::AddDivider(content);
	const auto &settings = Core::App().settings();
	AddQueueButton(
		content,
		settings.playerRepeatModeValue() | rpl::map([](Media::RepeatMode mode)
		-> rpl::producer<QString> {
			switch (mode) {
			case Media::RepeatMode::One: return tr::lng_gramm_repeat_one();
			case Media::RepeatMode::All: return tr::lng_gramm_repeat_all();
			case Media::RepeatMode::None: return tr::lng_gramm_repeat_off();
			}
			Unexpected("Music repeat mode.");
		}) | rpl::flatten_latest(),
		[] {
			auto &settings = Core::App().settings();
			switch (settings.playerRepeatMode()) {
			case Media::RepeatMode::None:
				settings.setPlayerRepeatMode(Media::RepeatMode::One);
				break;
			case Media::RepeatMode::One:
				settings.setPlayerRepeatMode(Media::RepeatMode::All);
				break;
			case Media::RepeatMode::All:
				settings.setPlayerRepeatMode(Media::RepeatMode::None);
				break;
			}
			Core::App().saveSettingsDelayed();
		});
	Ui::AddSubsectionTitle(content, tr::lng_gramm_queue_title());
	const auto list = content->add(object_ptr<Ui::VerticalLayout>(content));
	const auto player = Media::Player::instance();
	const auto session = &controller->session();
	const auto clear = AddQueueButton(
		content,
		tr::lng_gramm_music_clear(),
		[=] { player->musicQueue(session).clear(); });
	Ui::AddDividerText(content, tr::lng_gramm_music_about());
	Ui::AddDivider(content);
	Ui::AddSubsectionTitle(content, tr::lng_gramm_history_title());
	const auto recent = content->add(object_ptr<Ui::VerticalLayout>(content));
	const auto clearRecent = AddQueueButton(
		content,
		tr::lng_gramm_history_clear(),
		[=] { player->musicQueue(session).clearRecent(); });

	const auto refreshRecent = [=] {
		recent->clear();
		const auto &queue = player->musicQueue(session);
		clearRecent->setVisible(!queue.recent().empty());
		if (queue.recent().empty()) {
			Ui::AddDividerText(recent, tr::lng_gramm_history_empty());
			return;
		}
		for (const auto track : queue.recent()) {
			if (const auto audio = queue.resolve(track)) {
				AddQueueButton(recent, rpl::single(
					Ui::Text::FormatSongNameFor(audio.audio()).string()), [=] {
					if (const auto ready = player->musicQueue(session).resolve(track)) {
						player->play(ready);
					}
				});
			}
		}
	};
	player->musicQueue(session).recentChanges() | rpl::on_next([=] {
		Ui::PostponeCall(crl::guard(recent, refreshRecent));
	}, lifetime());
	refreshRecent();

	const auto refresh = [=] {
		list->clear();
		const auto &queue = player->musicQueue(session);
		const auto current = player->current(kSong);
		clear->setVisible(!queue.tracks().empty());
		if (queue.tracks().empty()) {
			Ui::AddDividerText(list, tr::lng_gramm_music_empty());
			return;
		}
		for (const auto track : queue.tracks()) {
			const auto audio = queue.resolve(track);
			const auto name = audio
				? Ui::Text::FormatSongNameFor(audio.audio()).string()
				: tr::lng_gramm_music_unavailable(tr::now);
			const auto isCurrent = audio
				&& (current.audio() == audio.audio())
				&& (current.contextId() == audio.contextId());
			auto label = rpl::producer<QString>();
			if (isCurrent) {
				label = tr::lng_gramm_music_current(lt_track, rpl::single(name));
			} else {
				label = rpl::single(name);
			}
			AddQueueButton(list, std::move(label), [=] {
				ShowQueueTrack(controller, track, name);
			});
		}
	};
	player->musicQueue(session).changes() | rpl::on_next(refresh, lifetime());
	player->trackChanged() | rpl::filter([](AudioMsgId::Type type) {
		return (type == kSong);
	}) | rpl::on_next(refresh, lifetime());
	refresh();
	Ui::ResizeFitChild(this, content);
}

rpl::producer<QString> MusicPlus::title() {
	return tr::lng_gramm_music();
}

} // namespace

Type GrammMusicId() {
	return MusicPlus::Id();
}

} // namespace Settings
