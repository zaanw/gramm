/*
This file is part of Gramm, based on Telegram Desktop.

For license and copyright information see the repository's LEGAL file.
*/
#include "settings/sections/settings_gramm.h"

#include "lang/lang_keys.h"
#include "media/player/media_player_instance.h"
#include "media/player/media_player_music_queue.h"
#include "settings/settings_common_session.h"
#include "ui/layers/generic_box.h"
#include "ui/text/format_song_document_name.h"
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

class Gramm final : public Section<Gramm> {
public:
	Gramm(QWidget *parent, not_null<Window::SessionController*> controller);

	[[nodiscard]] rpl::producer<QString> title() override;
};

Gramm::Gramm(
		QWidget *parent,
		not_null<Window::SessionController*> controller)
: Section(parent, controller) {
	const auto content = Ui::CreateChild<Ui::VerticalLayout>(this);
	Ui::AddDivider(content);
	Ui::AddSubsectionTitle(content, tr::lng_gramm_music());
	const auto list = content->add(object_ptr<Ui::VerticalLayout>(content));
	const auto player = Media::Player::instance();
	const auto session = &controller->session();
	const auto clear = AddQueueButton(
		content,
		tr::lng_gramm_music_clear(),
		[=] { player->musicQueue(session).clear(); });
	Ui::AddDividerText(content, tr::lng_gramm_music_about());

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

rpl::producer<QString> Gramm::title() {
	return tr::lng_gramm_settings();
}

} // namespace

Type GrammId() {
	return Gramm::Id();
}

} // namespace Settings
