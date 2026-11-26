/*
This file is part of Gramm, based on Telegram Desktop.

For license and copyright information see the repository's LEGAL file.
*/
#pragma once

#include "data/data_audio_msg_id.h"

#include <vector>

namespace Main {
class Session;
} // namespace Main

namespace Media::Player {

struct MusicQueueTrack {
	FullMsgId message;
	DocumentId document = 0;

	bool operator==(const MusicQueueTrack &other) const = default;
};

class MusicQueue final {
public:
	explicit MusicQueue(not_null<Main::Session*> session);

	[[nodiscard]] bool enqueue(
		MusicQueueTrack track,
		const AudioMsgId &current,
		bool playNext);
	void remove(MusicQueueTrack track);
	void move(MusicQueueTrack track, int delta);
	void clear();
	void remember(const AudioMsgId &id);
	void clearRecent();

	[[nodiscard]] const std::vector<MusicQueueTrack> &tracks() const;
	[[nodiscard]] const std::vector<MusicQueueTrack> &recent() const;
	[[nodiscard]] rpl::producer<> recentChanges() const;
	[[nodiscard]] AudioMsgId resolve(MusicQueueTrack track) const;
	[[nodiscard]] AudioMsgId adjacent(
		const AudioMsgId &current,
		int delta,
		bool wrap) const;
	[[nodiscard]] rpl::producer<> changes() const;
	[[nodiscard]] rpl::lifetime &lifetime();

private:
	[[nodiscard]] MusicQueueTrack key(const AudioMsgId &id) const;

	not_null<Main::Session*> _session;
	std::vector<MusicQueueTrack> _tracks;
	std::vector<MusicQueueTrack> _recent;
	MusicQueueTrack _lastRemembered;
	rpl::event_stream<> _changes;
	rpl::event_stream<> _recentChanges;
	rpl::lifetime _lifetime;
};

} // namespace Media::Player
