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

#include "audio/audiostream.h"
#include "audio/decoders/mp3.h"
#include "audio/mixer.h"
#include "common/debug.h"
#include "common/file.h"
#include "common/path.h"

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
