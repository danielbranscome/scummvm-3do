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

#include "sherlock/scalpel/scalpel_scene_audio.h"
#include "sherlock/scalpel/scalpel.h"

#include <cstring>

#include "audio/audiostream.h"
#include "audio/decoders/mp3.h"
#include "audio/mixer.h"
#include "common/debug.h"
#include "common/file.h"
#include "common/path.h"
#include "common/system.h"

namespace Sherlock {

namespace Scalpel {

static const char *kSceneAudioSubdir = "scene_audio/";

// Lookup table for (scene, cAnim) → filename.
// Currently 1 entry (Baker Street violin animation, cAnim 4 in room 4).
// Both meaningful Use paths on the violin object — Use-violin-alone via
// _use[1]._target=*SELF* AND Use-Music-on-violin via _use[0]._target=Music —
// dispatch through `ScalpelUserInterface::checkUseAction` to
// `ScalpelScene::startCAnim(action._cAnimNum - 1, ...)` with the same
// integer (cAnim 4), and both reach the Hook site (verified via runtime
// instrumentation, 2026-04-27), so this single entry covers both paths.
const SceneAudio::Entry SceneAudio::kEntries[] = {
	{ 4, 4, "baker_street_violin.mp3" },
};
const size_t SceneAudio::kEntryCount = sizeof(kEntries) / sizeof(kEntries[0]);

// Lookup table for named UI events → filename, used by playEvent().
// Currently 1 entry: "map_travel" — horse-and-carriage SFX triggered
// from `ScalpelMap::show()` at the destination-commit site.
const SceneAudio::EventEntry SceneAudio::kEventEntries[] = {
	{ "map_travel", "map_travel.mp3" },
};
const size_t SceneAudio::kEventEntryCount =
	sizeof(kEventEntries) / sizeof(kEventEntries[0]);

SceneAudio::SceneAudio(ScalpelEngine *vm) : _vm(vm) {
}

SceneAudio::~SceneAudio() {
	// Stop any in-flight playback before the engine tears down. Safe
	// regardless of mixer state — _mixer is owned by OSystem and
	// outlives the engine.
	stop();
}

bool SceneAudio::play(int scene, int cAnimNum) {
	const Entry *match = nullptr;
	for (size_t i = 0; i < kEntryCount; ++i) {
		const Entry &e = kEntries[i];
		if (e.scene == scene && e.cAnimNum == cAnimNum) {
			match = &e;
			break;
		}
	}
	if (match == nullptr) {
		// No entry for this tuple — silently ignore. Common case: the
		// vast majority of cAnim invocations have no SceneAudio entry.
		// Logging would spam.
		return false;
	}

	// Interrupt-on-new: stop the prior clip before starting the next.
	stop();

	Common::Path path = Common::Path(kSceneAudioSubdir).append(match->filename);
	Common::File *file = new Common::File();
	if (!file->open(path)) {
		warning("SceneAudio::play: could not open %s", path.toString().c_str());
		delete file;
		return false;
	}

	// makeMP3Stream takes ownership of the file via DisposeAfterUse::YES,
	// so even on failure here we don't double-free.
	Audio::SeekableAudioStream *stream = Audio::makeMP3Stream(file, DisposeAfterUse::YES);
	if (stream == nullptr) {
		warning("SceneAudio::play: makeMP3Stream returned null for %s",
		        path.toString().c_str());
		return false;
	}

	// kSFXSoundType so the user's SFX volume slider in ScummVM's audio
	// settings affects scene-SFX clips independently of narrator-VO
	// (kSpeechSoundType) and background music (kMusicSoundType, where
	// Osprey's looping MUSIC.LIB lives).
	_vm->_mixer->playStream(Audio::Mixer::kSFXSoundType, &_activeHandle, stream);
	debug(2, "SceneAudio::play: %s (scene=%d cAnim=%d)",
	      match->filename, scene, cAnimNum);
	return true;
}

bool SceneAudio::playEvent(const char *eventName) {
	const EventEntry *match = nullptr;
	for (size_t i = 0; i < kEventEntryCount; ++i) {
		const EventEntry &e = kEventEntries[i];
		if (strcmp(e.eventName, eventName) == 0) {
			match = &e;
			break;
		}
	}
	if (match == nullptr) {
		// No entry for this event name — silently ignore. Logging would
		// be misleading (the call site may dispatch many events, only
		// some of which have audio).
		return false;
	}

	// Interrupt-on-new: stop the prior clip before starting the next.
	stop();

	Common::Path path = Common::Path(kSceneAudioSubdir).append(match->filename);
	Common::File *file = new Common::File();
	if (!file->open(path)) {
		warning("SceneAudio::playEvent: could not open %s", path.toString().c_str());
		delete file;
		return false;
	}

	Audio::SeekableAudioStream *stream = Audio::makeMP3Stream(file, DisposeAfterUse::YES);
	if (stream == nullptr) {
		warning("SceneAudio::playEvent: makeMP3Stream returned null for %s",
		        path.toString().c_str());
		return false;
	}

	_vm->_mixer->playStream(Audio::Mixer::kSFXSoundType, &_activeHandle, stream);
	debug(2, "SceneAudio::playEvent: %s (event=%s)", match->filename, eventName);
	return true;
}

void SceneAudio::fadeOut(uint32 durationMs) {
	if (!isPlaying())
		return;
	if (durationMs < 50) {
		stop();
		return;
	}

	// 16-step linear ramp. At 400 ms target that's 25 ms per step —
	// smooth enough for the ear to hear a continuous fade.
	const int kSteps = 16;
	const uint32 stepMs = durationMs / kSteps;
	for (int i = kSteps - 1; i >= 0; --i) {
		const byte vol = (byte)((Audio::Mixer::kMaxChannelVolume * i) / kSteps);
		_vm->_mixer->setChannelVolume(_activeHandle, vol);
		g_system->delayMillis(stepMs);
	}
	stop();
}

void SceneAudio::stop() {
	if (_vm->_mixer->isSoundHandleActive(_activeHandle)) {
		_vm->_mixer->stopHandle(_activeHandle);
	}
}

bool SceneAudio::isPlaying() const {
	return _vm->_mixer->isSoundHandleActive(_activeHandle);
}

} // End of namespace Scalpel

} // End of namespace Sherlock
