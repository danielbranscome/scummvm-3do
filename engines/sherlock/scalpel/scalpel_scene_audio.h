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

	// Static lookup table. Linear-scanned by play() — single-digit
	// entry count expected; if the table grows large, switch to a
	// HashMap keyed on a packed (scene, cAnim) pair.
	static const Entry kEntries[];
	static const size_t kEntryCount;

	ScalpelEngine *_vm;
	Audio::SoundHandle _activeHandle;
};

} // End of namespace Scalpel

} // End of namespace Sherlock

#endif
