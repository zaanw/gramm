/*
This file is part of Gramm, based on Telegram Desktop.

For license and copyright information see the repository's LEGAL file.
*/
#include "media/player/media_player_music_queue.h"

#include "data/data_document.h"
#include "data/data_media_types.h"
#include "data/data_session.h"
#include "history/history_item.h"
#include "history/history_item_helpers.h"
#include "main/main_account.h"
#include "main/main_session.h"
#include "media/player/media_player_instance.h"

#include <iterator>

namespace Media::Player {

MusicQueue::MusicQueue(not_null<Main::Session*> session)
: _session(session) {
	session->data().itemRemoved(
	) | rpl::on_next([=](not_null<const HistoryItem*> item) {
		if (std::erase_if(_tracks, [&](const MusicQueueTrack &track) {
				return (track.message == item->fullId());
			})) {
			_changes.fire({});
		}
	}, _lifetime);
}

bool MusicQueue::enqueue(
		MusicQueueTrack track,
		const AudioMsgId &current,
		bool playNext) {
	if (!resolve(track)) {
		return false;
	}
	const auto anchor = key(current);
	if (_tracks.empty() && (anchor != track) && resolve(anchor)) {
		_tracks.push_back(anchor);
	}
	const auto existing = ranges::find(_tracks, track);
	if (existing != end(_tracks)) {
		if (!playNext || track == anchor) {
			return false;
		}
		_tracks.erase(existing);
	}
	if (playNext) {
		const auto playing = ranges::find(_tracks, anchor);
		_tracks.insert(
			(playing == end(_tracks) ? begin(_tracks) : playing + 1),
			track);
	} else {
		_tracks.push_back(track);
	}
	_changes.fire({});
	return true;
}

void MusicQueue::remove(MusicQueueTrack track) {
	if (std::erase(_tracks, track)) {
		_changes.fire({});
	}
}

void MusicQueue::move(MusicQueueTrack track, int delta) {
	Expects(delta == -1 || delta == 1);

	const auto from = ranges::find(_tracks, track) - begin(_tracks);
	const auto size = std::ssize(_tracks);
	const auto to = from + delta;
	if (from < size && to >= 0 && to < size) {
		std::swap(_tracks[from], _tracks[to]);
		_changes.fire({});
	}
}

void MusicQueue::clear() {
	if (!_tracks.empty()) {
		_tracks.clear();
		_changes.fire({});
	}
}

const std::vector<MusicQueueTrack> &MusicQueue::tracks() const {
	return _tracks;
}

AudioMsgId MusicQueue::resolve(MusicQueueTrack track) const {
	const auto item = track.message
		? _session->data().message(track.message)
		: nullptr;
	if (!item || !IsRealPlaybackContext(item)) {
		return {};
	}
	const auto media = item->media();
	if (!media || media->ttlSeconds()) {
		return {};
	}
	const auto makeAudioId = [&](DocumentData *document) {
		if (!document || document->id != track.document) {
			return AudioMsgId();
		}
		const auto result = AudioMsgId(document, track.message);
		return (result.type() == AudioMsgId::Type::Song)
			? result
			: AudioMsgId();
	};
	if (const auto result = makeAudioId(media->document())) {
		return result;
	}
	for (const auto &document : ItemRichPageAudio(item)) {
		if (const auto result = makeAudioId(document)) {
			return result;
		}
	}
	return {};
}

AudioMsgId MusicQueue::adjacent(
		const AudioMsgId &current,
		int delta,
		bool wrap) const {
	Expects(delta == -1 || delta == 1);

	const auto size = std::ssize(_tracks);
	const auto playing = ranges::find(_tracks, key(current))
		- begin(_tracks);
	auto index = (playing < size) ? playing : (delta > 0 ? -1 : 0);
	for (auto remaining = size; remaining > 0; --remaining) {
		index += delta;
		if (index < 0 || index == size) {
			if (!wrap) {
				break;
			}
			index = (index < 0) ? size - 1 : 0;
		}
		if (const auto audio = resolve(_tracks[index])) {
			return audio;
		}
	}
	return {};
}

rpl::producer<> MusicQueue::changes() const {
	return _changes.events();
}

rpl::lifetime &MusicQueue::lifetime() {
	return _lifetime;
}

MusicQueueTrack MusicQueue::key(const AudioMsgId &id) const {
	const auto document = id.audio();
	return (document && (&document->session() == _session.get()))
		? MusicQueueTrack{ id.contextId(), document->id }
		: MusicQueueTrack{};
}

MusicQueue &Instance::musicQueue(not_null<Main::Session*> session) {
	auto &entry = _musicQueues[session.get()];
	if (!entry) {
		entry = std::make_unique<MusicQueue>(session);
		entry->changes() | rpl::on_next([=] {
			_musicQueueChanges.fire({});
		}, entry->lifetime());
		session->account().sessionChanges() | rpl::on_next([=] {
			_musicQueues.erase(session.get());
		}, entry->lifetime());
	}
	return *entry;
}

const MusicQueue *Instance::activeMusicQueue(
		not_null<const Data*> data) const {
	if (data->type != AudioMsgId::Type::Song) {
		return nullptr;
	}
	const auto i = _musicQueues.find(data->session);
	return (i != end(_musicQueues) && !i->second->tracks().empty())
		? i->second.get()
		: nullptr;
}

} // namespace Media::Player
