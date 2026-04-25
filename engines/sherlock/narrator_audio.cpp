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

#include "sherlock/narrator_audio.h"
#include "sherlock/sherlock.h"

#include "common/debug.h"
#include "common/file.h"
#include "common/formats/json.h"

namespace Sherlock {

static const char *kManifestRelPath = "narrator_audio/manifest.json";
static const char *kAudioSubdir     = "narrator_audio/";

NarratorAudio::NarratorAudio(SherlockEngine *vm) : _vm(vm), _loaded(false) {
}

NarratorAudio::~NarratorAudio() {
}

bool NarratorAudio::load() {
	if (_loaded) {
		return true;  // already populated
	}

	Common::File file;
	if (!file.open(Common::Path(kManifestRelPath))) {
		warning("NarratorAudio: %s not found in game directory", kManifestRelPath);
		return false;
	}
	uint32 size = file.size();
	if (size == 0) {
		warning("NarratorAudio: %s is empty", kManifestRelPath);
		file.close();
		return false;
	}

	// Slurp into a NUL-terminated buffer for the JSON parser.
	char *buf = new char[size + 1];
	uint32 readBytes = file.read(buf, size);
	file.close();
	if (readBytes != size) {
		warning("NarratorAudio: short read on %s (%u of %u bytes)",
		        kManifestRelPath, readBytes, size);
		delete[] buf;
		return false;
	}
	buf[size] = '\0';

	Common::JSONValue *root = Common::JSON::parse(buf);
	delete[] buf;
	if (root == nullptr) {
		warning("NarratorAudio: %s did not parse as JSON", kManifestRelPath);
		return false;
	}
	if (!root->isArray()) {
		warning("NarratorAudio: %s root is not a JSON array", kManifestRelPath);
		delete root;
		return false;
	}

	const Common::JSONArray &arr = root->asArray();
	uint32 totalRows = 0;
	uint32 placeholderRows = 0;
	uint32 missingFileRows = 0;
	uint32 unknownRoleRows = 0;

	for (Common::JSONArray::const_iterator it = arr.begin(); it != arr.end(); ++it) {
		++totalRows;
		Common::JSONValue *row = *it;
		if (row == nullptr || !row->isObject()) {
			continue;
		}

		// id
		if (!row->hasChild("id")) continue;
		Common::JSONValue *idVal = row->child("id");
		if (idVal == nullptr || !idVal->isString()) continue;
		Common::String entryId = idVal->asString();
		if (entryId.empty()) continue;

		// text — used to detect %s placeholders
		Common::String text;
		if (row->hasChild("text") && row->child("text") != nullptr
		    && row->child("text")->isString()) {
			text = row->child("text")->asString();
		}
		if (text.contains("%s")) {
			++placeholderRows;
			continue;  // 6 Watson connectives — no audio by design
		}

		// voice_role
		VoiceRole role = kRoleUnknown;
		if (row->hasChild("voice_role") && row->child("voice_role") != nullptr
		    && row->child("voice_role")->isString()) {
			Common::String roleStr = row->child("voice_role")->asString();
			if (roleStr == "narrator") {
				role = kRoleNarrator;
			} else if (roleStr == "watson") {
				role = kRoleWatson;
			}
		}
		if (role == kRoleUnknown) {
			++unknownRoleRows;
			continue;
		}

		// Verify the audio file actually exists alongside the manifest.
		// Skips orphan manifest rows so callers can trust hasEntry().
		Common::String filename = entryId + ".mp3";
		Common::Path audioPath = Common::Path(kAudioSubdir).append(filename);
		if (!Common::File::exists(audioPath)) {
			++missingFileRows;
			continue;
		}

		Entry e;
		e.role = role;
		e.filename = filename;
		_entries[entryId] = e;
	}

	delete root;

	debug("NarratorAudio: loaded %u entries from %s (manifest had %u rows; "
	      "%u %%s placeholders skipped, %u missing-file skipped, %u unknown-role skipped)",
	      _entries.size(), kManifestRelPath, totalRows,
	      placeholderRows, missingFileRows, unknownRoleRows);

	_loaded = (_entries.size() > 0);
	if (!_loaded) {
		warning("NarratorAudio: %s parsed but produced zero usable entries",
		        kManifestRelPath);
	}
	return _loaded;
}

Common::Path NarratorAudio::lookupAudioPath(const Common::String &entryId) const {
	Common::HashMap<Common::String, Entry>::const_iterator it = _entries.find(entryId);
	if (it == _entries.end()) {
		return Common::Path();
	}
	return Common::Path(kAudioSubdir).append(it->_value.filename);
}

NarratorAudio::VoiceRole NarratorAudio::lookupVoiceRole(const Common::String &entryId) const {
	Common::HashMap<Common::String, Entry>::const_iterator it = _entries.find(entryId);
	if (it == _entries.end()) {
		return kRoleUnknown;
	}
	return it->_value.role;
}

} // End of namespace Sherlock
