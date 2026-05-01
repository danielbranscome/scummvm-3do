/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef SHERLOCK_SCALPEL_SCENE_AUDIO_H
#define SHERLOCK_SCALPEL_SCENE_AUDIO_H

#include "audio/mixer.h"
#include "common/str.h"

namespace Sherlock {

namespace Scalpel {

class ScalpelEngine;

/**
 * Triggered SFX events tied to (scene, cAnim) tuples.
 *
 * Distinct from `Sherlock::NarratorAudio` (which routes through
 * `Audio::Mixer::kSpeechSoundType` for narrator-VO clips). SceneAudio
 * routes through `kSFXSoundType` so it coexists cleanly with both
 * narrator speech AND looping background music (Osprey's MUSIC.LIB
 * patch lives on `kMusicSoundType`).
 *
 * Lookup is a static `(scene, cAnim) → filename` table — currently
 * one entry (the Baker Street violin animation, cAnim 4 in room 4).
 * Future SFX events are 1-line additions to `kEntries` (and a
 * matching asset under `mod_assets/scene_audio/` plus an entry in
 * `SCENE_AUDIO_ASSETS` in `tools/deploy_narrator_audio.py`).
 *
 * The (scene, cAnim) keying is deliberate: SceneAudio fires from
 * inside `ScalpelScene::startCAnim`, just before the frame-by-frame
 * playback loop begins — i.e., AFTER Holmes's walk-to-target
 * completes. This means audio onset aligns with the visible
 * animation onset regardless of where Holmes was when the action
 * was dispatched. Hooking at (obj, verb) dispatch time would fire
 * before the walk, breaking alignment.
 *
 * Audio files are deployed to `<gamedir>/scene_audio/<filename>` by
 * the deploy script and read at playback time via standard
 * `Common::File::open()` paths.
 */
class SceneAudio {
public:
	SceneAudio(ScalpelEngine *vm);
	~SceneAudio();

	/**
	 * Play the SFX clip associated with `(scene, cAnimNum)`. Returns
	 * true if a clip was started, false if no entry matches the tuple
	 * (or open/decode failed — warning logged in those cases).
	 *
	 * Called from `ScalpelScene::startCAnim` for every cAnim playback;
	 * a table miss is the common case and silently returns false.
	 *
	 * Interrupt-on-new: if a clip is already playing, it is stopped
	 * before the new one starts. Prevents pile-up on rapid-fire
	 * triggers (e.g. repeated Use clicks on the violin).
	 *
	 * Routed through `Audio::Mixer::kSFXSoundType` so the user's SFX
	 * volume slider in ScummVM's audio settings affects scene SFX
	 * independently of narrator-VO and background music.
	 *
	 * Non-blocking — returns immediately; the mixer drains the stream
	 * on its own thread.
	 */
	bool play(int scene, int cAnimNum);

	/**
	 * Play the SFX clip associated with a named global UI event (i.e.,
	 * one not tied to a (scene, cAnim) pair). Returns true if a clip
	 * was started, false if no entry matches `eventName` or open/decode
	 * failed (warning logged in those cases).
	 *
	 * Same fire-and-forget semantics as play(int, int): kSFXSoundType,
	 * interrupt-on-new, non-blocking.
	 *
	 * Used for events fired from the UI layer rather than scene
	 * animations — currently `"map_travel"` (horse-and-carriage SFX
	 * triggered by destination selection on the global travel map).
	 * Lookup table is `kEventEntries[]` below; adding a new event is
	 * a 1-line addition there + a matching asset under
	 * `mod_assets/scene_audio/` + an entry in `SCENE_AUDIO_ASSETS` in
	 * `tools/deploy_narrator_audio.py`.
	 */
	bool playEvent(const char *eventName);

	/**
	 * Fade out the active clip linearly over `durationMs`, then stop.
	 * No-op if nothing is playing. For very short durations (< 50 ms),
	 * skips the ramp and just stops abruptly.
	 *
	 * Synchronous ramp via `setChannelVolume` + `delayMillis`. The mixer
	 * runs on a separate thread so audio plays through the fade; the
	 * main-thread block is acceptable during scene transitions where the
	 * caller has already finished its visual work and the screen is
	 * static during the fade window (e.g., the map screen lingering
	 * after `_map->show()` returns at `scalpel.cpp:1085`).
	 *
	 * Used at scene transitions to avoid abrupt audio cuts (e.g., the
	 * map-travel SFX faded as the destination scene's audio context
	 * initializes).
	 */
	void fadeOut(uint32 durationMs);

	/** Stop the active SFX clip if any. Safe to call when nothing is playing. */
	void stop();

	/** True iff an SFX clip is currently mid-playback. */
	bool isPlaying() const;

private:
	struct Entry {
		int scene;
		int cAnimNum;          // 0-indexed, matches startCAnim() parameter
		const char *filename;  // relative to scene_audio/ subdirectory
	};

	// (scene, cAnim)-keyed entries, used by play(int, int).
	struct EventEntry {
		const char *eventName;  // matched against playEvent() argument
		const char *filename;   // relative to scene_audio/ subdirectory
	};

	// Named-event entries, used by playEvent(const char *).

	// Static lookup tables. Linear-scanned — single-digit entry counts
	// expected for both. If either table grows large, switch to a
	// HashMap keyed on a packed (scene, cAnim) pair / on the event-name
	// string respectively.
	static const Entry kEntries[];
	static const size_t kEntryCount;
	static const EventEntry kEventEntries[];
	static const size_t kEventEntryCount;

	ScalpelEngine *_vm;
	Audio::SoundHandle _activeHandle;
};

} // End of namespace Scalpel

} // End of namespace Sherlock

#endif
