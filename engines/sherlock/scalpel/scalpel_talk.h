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

#ifndef SHERLOCK_SCALPEL_TALK_H
#define SHERLOCK_SCALPEL_TALK_H

#include "common/scummsys.h"
#include "common/array.h"
#include "common/rect.h"
#include "common/serializer.h"
#include "common/stream.h"
#include "common/stack.h"
#include "sherlock/talk.h"

namespace Video {
class ThreeDOMovieDecoder;
}

namespace Sherlock {

namespace Scalpel {

class ScalpelTalk : public Talk {
private:
	Common::Stack<SequenceEntry> _sequenceStack;

	// 011_SH narrator-VO mod (dialogue UX): _speechDecoder persists across
	// waitForMoreWithSpeech() calls so that same-speaker text-page advances
	// (click-to-advance within one continuous utterance) do not interrupt
	// the underlying audio. The decoder is owned by ScalpelTalk and torn
	// down at speaker boundaries, end-of-script, scene transitions, and
	// destruction. _speechAudioEndedAt is millis-tracked for the 700ms
	// portrait dismiss grace once audio finishes naturally.
	Video::ThreeDOMovieDecoder *_speechDecoder;
	int _speechDecoderSpeaker;
	int _speechDecoderSubIndex;
	uint32 _speechAudioEndedAt;

	/**
	 * Close + delete the persistent speech decoder if alive. Idempotent.
	 * Called at every Talk-state-disturb point (speaker switch, end of
	 * script, freeTalkVars, destructor).
	 */
	void closeSpeechDecoder();

	/**
	 * Get the center position for the current speaker, if any
	 */
	Common::Point get3doPortraitPosition() const;

	OpcodeReturn cmdSwitchSpeaker(const byte *&str);
	OpcodeReturn cmdAssignPortraitLocation(const byte *&str);
	OpcodeReturn cmdGotoScene(const byte *&str);
	OpcodeReturn cmdCallTalkFile(const byte *&str);
	OpcodeReturn cmdClearInfoLine(const byte *&str);
	OpcodeReturn cmdClearWindow(const byte *&str);
	OpcodeReturn cmdDisplayInfoLine(const byte *&str);
	OpcodeReturn cmdElse(const byte *&str);
	OpcodeReturn cmdIf(const byte *&str);
	OpcodeReturn cmdMoveMouse(const byte *&str);
	OpcodeReturn cmdPlayPrologue(const byte *&str);
	OpcodeReturn cmdRemovePortrait(const byte *&str);
	OpcodeReturn cmdSfxCommand(const byte *&str);
	OpcodeReturn cmdSummonWindow(const byte *&str);
	OpcodeReturn cmdWalkToCoords(const byte *&str);
protected:
	/**
	 * Display the talk interface window
	 */
	void talkInterface(const byte *&str) override;

	/**
	 * Pause when displaying a talk dialog on-screen
	 */
	void talkWait(const byte *&str) override;

	/**
	 * Called when the active speaker is switched
	 */
	void switchSpeaker() override;

	/**
	 * Called when a character being spoken to has no talk options to display
	 */
	void nothingToSay() override;

	/**
	 * Show the talk display
	 */
	void showTalk() override;
public:
	ScalpelTalk(SherlockEngine *vm);
	~ScalpelTalk() override;

	/**
	 * Override to also tear down the persistent speech decoder. Called from
	 * Scene::freeScene at every scene transition.
	 */
	void freeTalkVars() override;

	/**
	 * Public wrapper for closeSpeechDecoder() so non-friend call sites
	 * (Talk::doScript end, OP_SWITCH_SPEAKER opcode handler in base Talk,
	 * UI statement-to-reply transition) can invoke teardown.
	 */
	void stopSpeechDecoder() { closeSpeechDecoder(); }

	Common::String _fixedTextWindowExit;
	Common::String _fixedTextWindowUp;
	Common::String _fixedTextWindowDown;

	/**
	 * Opens the talk file 'talk.tlk' and searches the index for the specified
	 * conversation. If found, the data for that conversation is loaded
	 */
	void loadTalkFile(const Common::String &filename) override;

	/**
	 * Called whenever a conversation or item script needs to be run. For standard conversations,
	 * it opens up a description window similar to how 'talk' does, but shows a 'reply' directly
	 * instead of waiting for a statement option.
	 * @remarks		It seems that at some point, all item scripts were set up to use this as well.
	 *	In their case, the conversation display is simply suppressed, and control is passed on to
	 *	doScript to implement whatever action is required.
	 */
	void talkTo(const Common::String &filename) override;

	/**
	 * When the talk window has been displayed, waits a period of time proportional to
	 * the amount of text that's been displayed
	 */
	int waitForMore(int delay) override;

	/**
	 * Draws the interface for conversation display
	 */
	void drawInterface() override;

	/**
	 * Display a list of statements in a window at the bottom of the screen that the
	 * player can select from.
	 */
	bool displayTalk(bool slamIt) override;

	/**
	 * Prints a single conversation option in the interface window
	 */
	int talkLine(int lineNum, int stateNum, byte color, int lineY, bool slamIt) override;

	/**
	 * Trigger to play a 3DO talk dialog movie
	 */
	bool talk3DOMovieTrigger(int subIndex);

	/**
	 * Wait for more with 3DO speech audio on PC version
	 */
	int waitForMoreWithSpeech(int delay, int subIndex);

	/**
	 * Handles skipping over bad text in conversations
	 */
	static void skipBadText(const byte *&msgP);

	/**
	 * Push the details of a passed object onto the saved sequences stack
	 */
	void pushSequenceEntry(Object *obj) override;

	/**
	 * Pulls a background object sequence from the sequence stack and restore's the
	 * object's sequence
	 */
	void pullSequence(int slot = -1) override;

	/**
	 * Returns true if the script stack is empty
	 */
	bool isSequencesEmpty() const override { return _sequenceStack.empty(); }

	/**
	 * Clears the stack of pending object sequences associated with speakers in the scene
	 */
	void clearSequences() override;
};

} // End of namespace Scalpel

} // End of namespace Sherlock

#endif
