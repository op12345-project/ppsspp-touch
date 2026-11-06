// Copyright (c) 2012- PPSSPP Project.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0 or later.

#pragma once

#include <cmath>
#include <cstdint>
#include <cstring>
#include <string_view>

#include "UI/TouchNavigation.h"

// Duel state for ULES01183 v1.02, measured with paused debugger captures across boots, savestates,
// and Normal and Tag Free Duels. Addresses and bases are only trusted for that exact disc version.
// Everything here is pure: guest memory is only read through a callback, and HandActionPlanner only
// names buttons for its caller to pulse. Nothing writes guest memory.
namespace TagForce3Duel {

struct ExpectedModule {
	const char *name;
	uint32_t base;
	// Address of the module's name in its own data, as little-endian words. Code is not used for
	// this because the JIT can patch instructions in RAM.
	uint32_t nameAddress;
	uint32_t nameWords[3];
};

constexpr ExpectedModule EXPECTED_MODULES[] = {
	{ "modduel_mgr", 0x08E9AA00, 0x08EB5958, { 0x64646F6D, 0x5F6C6575, 0x0072676D } },
	{ "modduel_draw", 0x0983FB00, 0x098BF434, { 0x64646F6D, 0x5F6C6575, 0x77617264 } },
	{ "modduel_eng", 0x09BE1400, 0x09DE7268, { 0x64646F6D, 0x5F6C6575, 0x00676E65 } },
};

constexpr uint32_t CURSOR_X_ADDRESS = 0x098D5D20;
constexpr uint32_t CURSOR_CODE_ADDRESS = 0x098D5D40;
constexpr uint32_t PROMPT_FLAGS_ADDRESS = 0x098E94D0;
constexpr uint32_t OVERLAY_ADDRESS = 0x09BDD0B4;
constexpr uint32_t HAND_COUNT_ADDRESS = 0x09E2A534;
constexpr uint32_t HAND_LIMIT_A_ADDRESS = 0x09E2D210;
constexpr uint32_t HAND_LIMIT_B_ADDRESS = 0x09E2D218;
constexpr uint32_t TARGET_FLAG_ADDRESS = 0x09E2D234;
constexpr uint32_t COMMAND_READY_ADDRESS = 0x09E2D254;
constexpr uint32_t DISCARD_FLAG_ADDRESS = 0x09E2D2E0;
constexpr uint32_t HAND_LIMIT_C_ADDRESS = 0x09E2D2E8;
constexpr uint32_t HAND_LIMIT_D_ADDRESS = 0x09E2D2F0;

// The End Phase hand-limit prompt. The discard flag alone isn't enough: Lightning Vortex's cost shows
// the same banner and flag, plus the target flag. These four fields matched in all nine captures of
// the prompt (three duels including Tag, with other decks and turns) and differed in all 25 others:
// equip target, Special Summon from hand, attack target, effect discard, normal, menus, opponent.
constexpr uint32_t HAND_LIMIT_A = 4;
constexpr uint32_t HAND_LIMIT_B = 1;
constexpr uint32_t HAND_LIMIT_C = 1;
constexpr uint32_t HAND_LIMIT_D = 0x800001;
constexpr uint32_t HAND_LIMIT_SIZE = 7;

constexpr uint32_t PLAYER_HAND_ZONE = 0x16;
constexpr uint32_t OPPONENT_HAND_ZONE = 0x17;
constexpr uint32_t MAX_ZONE = 0x1F;
constexpr uint32_t OVERLAY_NONE = 1;
constexpr uint32_t PROMPT_NORMAL = 0x87;
constexpr uint32_t PROMPT_SELECTION = 0xC7;

// Only sizes whose layout was measured. Others stay rejected until validated.
constexpr uint32_t MIN_HAND_SIZE = 4;
constexpr uint32_t MAX_HAND_SIZE = 7;

constexpr float SLOT_WORLD_SPACING = 65.0f;
constexpr float WORLD_TO_PSP_X = 0.7342f;
constexpr float HAND_CENTER_PSP_X = 313.80f;
constexpr float SLOT_HALF_WIDTH = SLOT_WORLD_SPACING * WORLD_TO_PSP_X * 0.5f;
constexpr float SELECTED_HALF_WIDTH = 26.5f;
constexpr float CARD_TOP = 219.0f;
constexpr float SELECTED_TOP = 201.0f;

// While a confirmed action starts, ready was seen to read 1 for about 2 vblanks at a time, with
// every other field unchanged. A single reading therefore can't establish hand selection.
constexpr uint32_t STABLE_VBLANKS = 12;

enum class State : uint8_t {
	UNSUPPORTED_GAME,
	MODULES_NOT_LOADED,
	READ_FAILED,
	OVERLAY_ACTIVE,
	NOT_READY,
	DISCARD_PROMPT,
	EFFECT_DISCARD_PROMPT,
	TARGET_PROMPT,
	UNKNOWN_PROMPT,
	CURSOR_NOT_IN_HAND,
	UNSUPPORTED_HAND_SIZE,
	INVALID_CURSOR,
	LAYOUT_NOT_SETTLED,
	SETTLING,
	HAND_SELECTION,
	DISCARD_SELECTION,
};

inline const char *StateName(State state) {
	switch (state) {
	case State::UNSUPPORTED_GAME: return "unsupported game";
	case State::MODULES_NOT_LOADED: return "duel modules not loaded";
	case State::READ_FAILED: return "read failed";
	case State::OVERLAY_ACTIVE: return "overlay active";
	case State::NOT_READY: return "not ready";
	case State::DISCARD_PROMPT: return "discard prompt";
	case State::EFFECT_DISCARD_PROMPT: return "effect discard prompt";
	case State::TARGET_PROMPT: return "target prompt";
	case State::UNKNOWN_PROMPT: return "unknown prompt";
	case State::CURSOR_NOT_IN_HAND: return "cursor not in hand";
	case State::UNSUPPORTED_HAND_SIZE: return "unsupported hand size";
	case State::INVALID_CURSOR: return "invalid cursor";
	case State::LAYOUT_NOT_SETTLED: return "layout not settled";
	case State::SETTLING: return "settling";
	case State::HAND_SELECTION: return "hand selection";
	case State::DISCARD_SELECTION: return "discard selection";
	}
	return "unknown";
}

struct Snapshot {
	State state = State::UNSUPPORTED_GAME;
	uint32_t cursorCode = 0;
	uint32_t overlay = 0;
	uint32_t ready = 0;
	uint32_t promptFlags = 0;
	uint32_t handCount = 0;
	float cursorX = 0.0f;
	// Valid only in HAND_SELECTION and DISCARD_SELECTION.
	int selectedIndex = -1;
};

// The game's world X for a hand slot (-163, -98, -33, 32, 97, 162 with six cards).
inline float SlotWorldX(int index, int handCount) {
	return std::floor((index - (handCount - 1) * 0.5f) * SLOT_WORLD_SPACING);
}

inline float SlotCenterPspX(int index, int handCount) {
	return WORLD_TO_PSP_X * SlotWorldX(index, handCount) + HAND_CENTER_PSP_X;
}

inline bool IsValidatedHandSize(uint32_t handCount) {
	return handCount >= MIN_HAND_SIZE && handCount <= MAX_HAND_SIZE;
}

template <typename IsLoaded, typename ReadWord>
bool DuelModulesPresent(IsLoaded isLoaded, ReadWord readWord) {
	for (const ExpectedModule &module : EXPECTED_MODULES) {
		uint32_t base = 0;
		if (!isLoaded(module.name, &base) || base != module.base) {
			return false;
		}
		for (int i = 0; i < 3; ++i) {
			uint32_t word = 0;
			if (!readWord(module.nameAddress + i * 4, &word) || word != module.nameWords[i]) {
				return false;
			}
		}
	}
	return true;
}

// isLoaded(name, &base) reports a currently active module. readWord(address, &value) is a bounded
// guest read. Nothing is read for another disc or version.
template <typename IsLoaded, typename ReadWord>
Snapshot Observe(std::string_view discID, std::string_view discVersion, IsLoaded isLoaded, ReadWord readWord) {
	Snapshot result;
	if (discID != "ULES01183" || discVersion != "1.02") {
		return result;
	}
	result.state = State::MODULES_NOT_LOADED;
	if (!DuelModulesPresent(isLoaded, readWord)) {
		return result;
	}

	uint32_t cursorXBits = 0;
	uint32_t targetFlag = 0;
	uint32_t discardFlag = 0;
	uint32_t handLimit[4] = {};
	if (!readWord(CURSOR_X_ADDRESS, &cursorXBits) || !readWord(CURSOR_CODE_ADDRESS, &result.cursorCode) ||
		!readWord(PROMPT_FLAGS_ADDRESS, &result.promptFlags) || !readWord(OVERLAY_ADDRESS, &result.overlay) ||
		!readWord(HAND_COUNT_ADDRESS, &result.handCount) || !readWord(TARGET_FLAG_ADDRESS, &targetFlag) ||
		!readWord(COMMAND_READY_ADDRESS, &result.ready) || !readWord(DISCARD_FLAG_ADDRESS, &discardFlag) ||
		!readWord(HAND_LIMIT_A_ADDRESS, &handLimit[0]) || !readWord(HAND_LIMIT_B_ADDRESS, &handLimit[1]) ||
		!readWord(HAND_LIMIT_C_ADDRESS, &handLimit[2]) || !readWord(HAND_LIMIT_D_ADDRESS, &handLimit[3])) {
		result.state = State::READ_FAILED;
		return result;
	}
	memcpy(&result.cursorX, &cursorXBits, sizeof(result.cursorX));

	const uint32_t zone = result.cursorCode & 0x3F;
	const uint32_t index = result.cursorCode >> 6;
	if (result.overlay != OVERLAY_NONE) {
		result.state = State::OVERLAY_ACTIVE;
	} else if (result.ready != 1) {
		result.state = State::NOT_READY;
	} else if (result.promptFlags == PROMPT_SELECTION) {
		const bool handLimitFields = handLimit[0] == HAND_LIMIT_A && handLimit[1] == HAND_LIMIT_B &&
			handLimit[2] == HAND_LIMIT_C && handLimit[3] == HAND_LIMIT_D;
		if (discardFlag == 1 && targetFlag == 0 && handLimitFields) {
			result.state = State::DISCARD_PROMPT;
			if (zone == PLAYER_HAND_ZONE && result.handCount == HAND_LIMIT_SIZE && index < result.handCount &&
				std::fabs(result.cursorX - SlotWorldX((int)index, (int)result.handCount)) <= 0.5f) {
				result.state = State::DISCARD_SELECTION;
				result.selectedIndex = (int)index;
			}
		} else if (discardFlag == 1 && targetFlag == 1) {
			result.state = State::EFFECT_DISCARD_PROMPT;
		} else if (targetFlag == 1 && discardFlag == 0) {
			result.state = State::TARGET_PROMPT;
		} else {
			result.state = State::UNKNOWN_PROMPT;
		}
	} else if (result.promptFlags != PROMPT_NORMAL) {
		result.state = State::UNKNOWN_PROMPT;
	} else if (zone > MAX_ZONE || (index != 0 && zone != PLAYER_HAND_ZONE && zone != OPPONENT_HAND_ZONE)) {
		result.state = State::INVALID_CURSOR;
	} else if (!IsValidatedHandSize(result.handCount)) {
		result.state = State::UNSUPPORTED_HAND_SIZE;
	} else if (zone != PLAYER_HAND_ZONE) {
		result.state = State::CURSOR_NOT_IN_HAND;
	} else if (index >= result.handCount) {
		result.state = State::INVALID_CURSOR;
	} else if (!(std::fabs(result.cursorX - SlotWorldX((int)index, (int)result.handCount)) <= 0.5f)) {
		// Cards animate into place after the hand changes size. Written so NaN is rejected.
		result.state = State::LAYOUT_NOT_SETTLED;
	} else {
		result.state = State::HAND_SELECTION;
		result.selectedIndex = (int)index;
	}
	return result;
}

// Holds back every actionable state until it has lasted STABLE_VBLANKS. Counts guest vblanks, so
// slowdown and fast-forward don't change what it means. Cursor moves, including between the hand and
// the board, don't restart it; leaving normal play or the discard prompt does.
class StabilityGate {
public:
	Snapshot Apply(const Snapshot &snapshot, uint64_t vblanks) {
		const bool normal = snapshot.state == State::HAND_SELECTION || snapshot.state == State::CURSOR_NOT_IN_HAND;
		const bool discard = snapshot.state == State::DISCARD_SELECTION;
		if (!normal && !discard) {
			Reset();
			return snapshot;
		}
		// Going backward means a savestate restored the counter.
		if (!tracking_ || vblanks < start_ || snapshot.handCount != handCount_ || discard != discard_) {
			tracking_ = true;
			start_ = vblanks;
			handCount_ = snapshot.handCount;
			discard_ = discard;
		}
		if (vblanks - start_ >= STABLE_VBLANKS) {
			return snapshot;
		}
		Snapshot settling = snapshot;
		settling.state = State::SETTLING;
		settling.selectedIndex = -1;
		return settling;
	}

	void Reset() {
		tracking_ = false;
	}

private:
	bool tracking_ = false;
	bool discard_ = false;
	uint64_t start_ = 0;
	uint32_t handCount_ = 0;
};

// Layout hit test in PSP coordinates. The selected card is raised, wider, and drawn over its
// neighbors; with the cursor on the board (selectedIndex -1) no card is. Returns the slot, or -1
// outside the cards or for an unvalidated hand size.
inline int HitTestSlot(float gameX, float gameY, uint32_t handCount, int selectedIndex) {
	if (!IsValidatedHandSize(handCount) || selectedIndex < -1 || selectedIndex >= (int)handCount) {
		return -1;
	}
	if (gameX < 0.0f || gameX >= 480.0f || gameY < SELECTED_TOP || gameY >= 272.0f) {
		return -1;
	}
	if (selectedIndex >= 0 && std::fabs(gameX - SlotCenterPspX(selectedIndex, (int)handCount)) <= SELECTED_HALF_WIDTH) {
		return selectedIndex;
	}
	if (gameY < CARD_TOP) {
		return -1;
	}
	for (int i = 0; i < (int)handCount; ++i) {
		if (std::fabs(gameX - SlotCenterPspX(i, (int)handCount)) <= SLOT_HALF_WIDTH) {
			return i;
		}
	}
	return -1;
}

inline bool IsActionable(const Snapshot &snapshot) {
	return snapshot.state == State::HAND_SELECTION || snapshot.state == State::DISCARD_SELECTION ||
		snapshot.state == State::CURSOR_NOT_IN_HAND;
}

// Pointer coordinates and viewport share the same pixel space. Only a snapshot that passed
// StabilityGate in an actionable state can produce a slot.
inline int HitTest(const Snapshot &snapshot, float x, float y, float viewportX, float viewportY, float viewportW, float viewportH) {
	float gameX = 0.0f;
	float gameY = 0.0f;
	if (!IsActionable(snapshot) ||
		!PointerToGameFrame(x, y, viewportX, viewportY, viewportW, viewportH, &gameX, &gameY)) {
		return -1;
	}
	const int selected = snapshot.state == State::CURSOR_NOT_IN_HAND ? -1 : snapshot.selectedIndex;
	if (snapshot.state != State::CURSOR_NOT_IN_HAND && selected < 0) {
		return -1;
	}
	return HitTestSlot(gameX, gameY, snapshot.handCount, selected);
}

// Turns taps on hand slots into verified D-pad pulses, and at most one Cross per request. It reads
// only gated snapshots, so a state that changes at any point cancels the action and releases what's
// held. Pulses are timed in guest vblanks so the game has really seen them, with a wall-clock limit
// so a stalled game can't leave one queued.
class HandActionPlanner {
public:
	enum class Button : uint8_t { NONE, LEFT, RIGHT, DOWN, CROSS };

	struct Step {
		Button press = Button::NONE;
		Button release = Button::NONE;
	};

	struct Context {
		Snapshot snapshot;
		bool focused = false;
		bool paused = false;
		bool otherInput = false;  // Physical buttons held through the normal mappings.
		uint32_t stateSerial = 0;
		uint64_t vblanks = 0;
		double wallTime = 0.0;
	};

	enum class TapResult : uint8_t { REJECTED, NAVIGATING, CONFIRMING, CONFIRM_ADDED, RETARGETED };

	enum class CancelReason : uint8_t { NONE, REQUESTED, FOCUS, PAUSED, STATE_LOAD, CLOCK, OTHER_INPUT, TIMEOUT, STATE_CHANGED, HAND_CHANGED, CURSOR };

	TapResult Tap(int slot, const Context &context) {
		if (slot < 0 || !Usable(context) || !IsActionable(context.snapshot) ||
			(uint32_t)slot >= context.snapshot.handCount) {
			return TapResult::REJECTED;
		}
		const bool discard = context.snapshot.state == State::DISCARD_SELECTION;
		if (phase_ != Phase::IDLE) {
			// A discard can't be redirected, and an X on its way can't be taken back.
			if (phase_ == Phase::CONFIRM || phase_ == Phase::COOLDOWN || discard_ || discard) {
				return TapResult::REJECTED;
			}
			if (slot == target_) {
				confirm_ = true;
				return TapResult::CONFIRM_ADDED;
			}
			target_ = slot;
			confirm_ = false;
			if (phase_ == Phase::SETTLE) {
				phase_ = Phase::NAVIGATE;
				cursor_.Start(context.snapshot.selectedIndex, slot, (int)handCount_, true, (int)handCount_, GuestSeconds(context));
			} else if (phase_ == Phase::NAVIGATE) {
				cursor_.Retarget(slot);
			}
			deadline_ = context.wallTime + DEADLINE_SECONDS;
			return TapResult::RETARGETED;
		}

		target_ = slot;
		discard_ = discard;
		confirm_ = discard;
		handCount_ = context.snapshot.handCount;
		stateSerial_ = context.stateSerial;
		lastVblanks_ = context.vblanks;
		deadline_ = context.wallTime + DEADLINE_SECONDS;
		lastCancel_ = CancelReason::NONE;
		if (context.snapshot.state == State::CURSOR_NOT_IN_HAND) {
			phase_ = Phase::ENTER_READY;
			enterSteps_ = 0;
			waitUntil_ = context.vblanks;
			lastCode_ = context.snapshot.cursorCode;
			return TapResult::NAVIGATING;
		}
		if (context.snapshot.selectedIndex == slot) {
			confirm_ = true;
			phase_ = Phase::SETTLE;
			waitUntil_ = context.vblanks;
			return TapResult::CONFIRMING;
		}
		phase_ = Phase::NAVIGATE;
		cursor_.Start(context.snapshot.selectedIndex, slot, (int)handCount_, true, (int)handCount_, GuestSeconds(context));
		return confirm_ ? TapResult::CONFIRMING : TapResult::NAVIGATING;
	}

	Step Update(const Context &context) {
		if (phase_ == Phase::IDLE) {
			return {};
		}
		if (phase_ == Phase::COOLDOWN) {
			// Holds back another tap until the game has reacted to X, or long enough that it won't.
			if (context.vblanks < lastVblanks_ || context.vblanks >= waitUntil_ || !Usable(context) ||
				!IsActionable(context.snapshot)) {
				phase_ = Phase::IDLE;
			}
			return {};
		}
		const CancelReason reason = Check(context);
		if (reason != CancelReason::NONE) {
			return Abort(reason);
		}
		lastVblanks_ = context.vblanks;
		switch (phase_) {
		case Phase::ENTER_READY:
			if (context.vblanks < waitUntil_) {
				return {};
			}
			if (context.snapshot.cursorCode != lastCode_ || ++enterSteps_ > MAX_ENTER_STEPS) {
				return Abort(CancelReason::CURSOR);
			}
			held_ = Button::DOWN;
			waitUntil_ = context.vblanks + PRESS_VBLANKS;
			phase_ = Phase::ENTER_PRESS;
			return { Button::DOWN, Button::NONE };
		case Phase::ENTER_PRESS:
			if (context.vblanks < waitUntil_) {
				return {};
			}
			held_ = Button::NONE;
			waitUntil_ = context.vblanks + RESPONSE_VBLANKS;
			phase_ = Phase::ENTER_WAIT;
			return { Button::NONE, Button::DOWN };
		case Phase::ENTER_WAIT:
			if (context.snapshot.cursorCode == lastCode_) {
				if (context.vblanks >= waitUntil_) {
					return Abort(CancelReason::CURSOR);
				}
				return {};
			}
			lastCode_ = context.snapshot.cursorCode;
			if (context.snapshot.state == State::HAND_SELECTION) {
				BeginNavigation(context);
			} else {
				waitUntil_ = context.vblanks + GAP_VBLANKS;
				phase_ = Phase::ENTER_READY;
			}
			return {};
		case Phase::NAVIGATE: {
			const CursorStep step = cursor_.Update(true, context.snapshot.selectedIndex, GuestSeconds(context));
			if (!cursor_.Active() && step.press == CursorDirection::NONE) {
				if (context.snapshot.selectedIndex != target_) {
					const Step released = ToStep(step);
					Abort(CancelReason::CURSOR);
					return released;
				}
				if (confirm_) {
					phase_ = Phase::SETTLE;
					waitUntil_ = context.vblanks + GAP_VBLANKS;
				} else {
					phase_ = Phase::IDLE;
				}
			}
			return ToStep(step);
		}
		case Phase::SETTLE:
			if (context.snapshot.selectedIndex != target_) {
				return Abort(CancelReason::CURSOR);
			}
			if (!confirm_) {
				phase_ = Phase::IDLE;
				return {};
			}
			if (context.vblanks < waitUntil_) {
				return {};
			}
			held_ = Button::CROSS;
			waitUntil_ = context.vblanks + PRESS_VBLANKS;
			phase_ = Phase::CONFIRM;
			return { Button::CROSS, Button::NONE };
		case Phase::CONFIRM:
			if (context.vblanks < waitUntil_) {
				return {};
			}
			held_ = Button::NONE;
			waitUntil_ = context.vblanks + COOLDOWN_VBLANKS;
			phase_ = Phase::COOLDOWN;
			return { Button::NONE, Button::CROSS };
		default:
			return {};
		}
	}

	Step Cancel(CancelReason reason = CancelReason::REQUESTED) {
		return Abort(reason);
	}

	bool Active() const { return phase_ != Phase::IDLE; }
	// True while a button is held or still to come, so other pointer input should wait.
	bool Driving() const { return phase_ != Phase::IDLE && phase_ != Phase::COOLDOWN; }
	// True while X is held. Stray pointer input shouldn't cut that pulse short.
	bool Committed() const { return phase_ == Phase::CONFIRM; }
	CancelReason LastCancel() const { return lastCancel_; }
	int Target() const { return target_; }

private:
	static constexpr uint64_t PRESS_VBLANKS = 5;
	static constexpr uint64_t GAP_VBLANKS = 2;
	static constexpr uint64_t RESPONSE_VBLANKS = 30;
	static constexpr uint64_t COOLDOWN_VBLANKS = 30;
	static constexpr int MAX_ENTER_STEPS = 6;
	static constexpr double DEADLINE_SECONDS = 3.0;

	enum class Phase : uint8_t { IDLE, ENTER_READY, ENTER_PRESS, ENTER_WAIT, NAVIGATE, SETTLE, CONFIRM, COOLDOWN };

	static double GuestSeconds(const Context &context) {
		return (double)context.vblanks / 60.0;
	}

	static bool Usable(const Context &context) {
		return context.focused && !context.paused && !context.otherInput;
	}

	static Button ToButton(CursorDirection direction) {
		return direction == CursorDirection::NEXT ? Button::RIGHT :
			direction == CursorDirection::PREVIOUS ? Button::LEFT : Button::NONE;
	}

	static Step ToStep(CursorStep step) {
		return { ToButton(step.press), ToButton(step.release) };
	}

	void BeginNavigation(const Context &context) {
		const int selected = context.snapshot.selectedIndex;
		cursor_.Start(selected, target_, (int)handCount_, true, (int)handCount_, GuestSeconds(context));
		phase_ = selected == target_ ? Phase::SETTLE : Phase::NAVIGATE;
		waitUntil_ = context.vblanks + GAP_VBLANKS;
	}

	CancelReason Check(const Context &context) const {
		if (!context.focused) {
			return CancelReason::FOCUS;
		}
		if (context.paused) {
			return CancelReason::PAUSED;
		}
		if (context.stateSerial != stateSerial_) {
			return CancelReason::STATE_LOAD;
		}
		if (context.vblanks < lastVblanks_) {
			return CancelReason::CLOCK;
		}
		if (context.otherInput) {
			return CancelReason::OTHER_INPUT;
		}
		if (context.wallTime > deadline_) {
			return CancelReason::TIMEOUT;
		}
		// X is on its way; only the release remains, and the game is expected to react.
		if (phase_ == Phase::CONFIRM) {
			return CancelReason::NONE;
		}
		const State state = context.snapshot.state;
		const bool entering = phase_ == Phase::ENTER_READY || phase_ == Phase::ENTER_PRESS || phase_ == Phase::ENTER_WAIT;
		const bool allowed = discard_ ? state == State::DISCARD_SELECTION :
			state == State::HAND_SELECTION || (entering && state == State::CURSOR_NOT_IN_HAND);
		if (!allowed) {
			return CancelReason::STATE_CHANGED;
		}
		if (context.snapshot.handCount != handCount_) {
			return CancelReason::HAND_CHANGED;
		}
		return CancelReason::NONE;
	}

	Step Abort(CancelReason reason) {
		Step step = ToStep(cursor_.Cancel());
		if (held_ != Button::NONE) {
			step.release = held_;
		}
		held_ = Button::NONE;
		if (phase_ != Phase::IDLE) {
			lastCancel_ = reason;
		}
		phase_ = Phase::IDLE;
		confirm_ = false;
		return step;
	}

	CursorPlanner cursor_;
	Phase phase_ = Phase::IDLE;
	Button held_ = Button::NONE;
	CancelReason lastCancel_ = CancelReason::NONE;
	int target_ = -1;
	int enterSteps_ = 0;
	bool confirm_ = false;
	bool discard_ = false;
	uint32_t handCount_ = 0;
	uint32_t stateSerial_ = 0;
	uint32_t lastCode_ = 0;
	uint64_t waitUntil_ = 0;
	uint64_t lastVblanks_ = 0;
	double deadline_ = 0.0;
};

// Field probe: read-only classification and hit testing of the 20 monster and Spell/Trap zones.
// Measured 2026-11-06 with paused captures in Normal and Tag Free Duels (see docs/CURRENT_STATE.md).
// Nothing here sends input.

// Selection block next to the cursor. Observed: attack target sets ATTACK, equip and other card choices
// set CARD, Yes/No dialogs set DIALOG. ANIM reads 1 for about 0.5 s while a target choice opens, 3
// while it's open, then 4, 2, 0 over about 0.6 s after one is confirmed; ready can be 1 throughout.
// CARD is also set by hand-only choices (an effect's discard cost, Special Summon from hand).
constexpr uint32_t SELECT_ATTACK_ADDRESS = 0x098D5D00;
constexpr uint32_t SELECT_CARD_ADDRESS = 0x098D5D04;
constexpr uint32_t SELECT_DIALOG_ADDRESS = 0x098D5D08;
constexpr uint32_t SELECT_ANIM_ADDRESS = 0x098D5D10;
constexpr uint32_t SELECT_ANIM2_ADDRESS = 0x098D5D14;
constexpr uint32_t CURSOR_COLUMN_ADDRESS = 0x098D5D44;
constexpr uint32_t CURSOR_ROW_ADDRESS = 0x098D5D48;
constexpr uint32_t SELECT_ANIM_OPEN = 3;

// Per-player zone tables: 0x14-byte entries, monster columns 0-4 then Spell/Trap columns 0-4, counted
// from that player's left. Word 0 is the card (low 14 bits the card ID), 0 when empty.
constexpr uint32_t PLAYER_FIELD_ADDRESS = 0x09E2A55C;
constexpr uint32_t OPPONENT_FIELD_ADDRESS = 0x09E2B098;
constexpr uint32_t FIELD_ENTRY_SIZE = 0x14;
constexpr uint32_t FIELD_CARD_ID_MASK = 0x3FFF;
constexpr int FIELD_ZONES = 0x14;

// Screen rows as the game numbers them (its own row word): opponent hand 0, opponent S/T 1, opponent
// monsters 2, the middle piles 3, player monsters 4, player S/T 5, player hand 6. Columns 0-6, with
// piles in 0 and 6.
constexpr int ROW_OPPONENT_HAND = 0;
constexpr int ROW_PLAYER_HAND = 6;

// Zone 0x00-0x13: even is the player's, odd the opponent's; below 0x0A monsters, then Spell/Trap.
inline bool IsFieldCardZone(uint32_t zone) {
	return zone < (uint32_t)FIELD_ZONES;
}

inline bool IsOpponentZone(uint32_t zone) {
	return (zone & 1) != 0;
}

// Column in the owner's own table, 0-4 from that player's left.
inline int OwnerColumn(uint32_t zone) {
	return zone < 0x0A ? (int)(zone >> 1) : (int)((zone - 0x0A) >> 1);
}

// Screen row and column of a zone, matching the game's row and column words. Piles included so the
// cursor can be cross-checked anywhere on the board. Returns false for hands and unknown zones.
inline bool ZoneGridPosition(uint32_t zone, int *row, int *column) {
	static const uint8_t ROWS[4][7] = {
		{ 0x1B, 0x13, 0x11, 0x0F, 0x0D, 0x0B, 0x19 },
		{ 0x1D, 0x09, 0x07, 0x05, 0x03, 0x01, 0x15 },
		{ 0x14, 0x00, 0x02, 0x04, 0x06, 0x08, 0x1C },
		{ 0x18, 0x0A, 0x0C, 0x0E, 0x10, 0x12, 0x1A },
	};
	static const int ROW_NUMBERS[4] = { 1, 2, 4, 5 };
	for (int r = 0; r < 4; ++r) {
		for (int c = 0; c < 7; ++c) {
			if (ROWS[r][c] == zone) {
				*row = ROW_NUMBERS[r];
				*column = c;
				return true;
			}
		}
	}
	if (zone == 0x1F || zone == 0x1E) {
		*row = 3;
		*column = zone == 0x1F ? 0 : 6;
		return true;
	}
	return false;
}

inline uint32_t FieldEntryAddress(uint32_t zone) {
	const uint32_t base = IsOpponentZone(zone) ? OPPONENT_FIELD_ADDRESS : PLAYER_FIELD_ADDRESS;
	const uint32_t entry = (zone < 0x0A ? 0 : 5) + (uint32_t)OwnerColumn(zone);
	return base + entry * FIELD_ENTRY_SIZE;
}

// Bounds of the highlight frame in PSP pixels, identical in Normal and Tag duels. Tiles are slanted,
// so neighbors overlap by about 4 px.
struct ZoneRect {
	float x1, y1, x2, y2;
};

inline bool FieldZoneRect(uint32_t zone, ZoneRect *rect) {
	int row = 0;
	int column = 0;
	if (!IsFieldCardZone(zone) || !ZoneGridPosition(zone, &row, &column) || column < 1 || column > 5) {
		return false;
	}
	static const float X[4][5][2] = {
		{ { 221.0f, 261.5f }, { 258.0f, 296.5f }, { 295.5f, 332.0f }, { 331.5f, 368.0f }, { 366.5f, 404.0f } },
		{ { 214.0f, 258.0f }, { 254.0f, 295.5f }, { 294.0f, 333.5f }, { 332.5f, 372.0f }, { 370.0f, 410.0f } },
		{ { 200.5f, 251.0f }, { 246.0f, 293.0f }, { 291.0f, 336.0f }, { 335.0f, 380.0f }, { 377.0f, 424.0f } },
		{ { 190.0f, 245.5f }, { 239.5f, 291.0f }, { 289.0f, 339.0f }, { 337.0f, 388.5f }, { 382.5f, 438.0f } },
	};
	static const float Y[4][2] = { { 76.0f, 97.5f }, { 98.0f, 123.0f }, { 141.0f, 172.5f }, { 173.5f, 211.0f } };
	const int r = row == 1 ? 0 : row == 2 ? 1 : row == 4 ? 2 : 3;
	*rect = { X[r][column - 1][0], Y[r][0], X[r][column - 1][1], Y[r][1] };
	return true;
}

// The zone under a PSP-frame point, from geometry alone; -1 if none. Overlaps go to the nearer center.
inline int FieldZoneAt(float gameX, float gameY) {
	int best = -1;
	float bestDistance = 0.0f;
	for (uint32_t zone = 0; zone < (uint32_t)FIELD_ZONES; ++zone) {
		ZoneRect rect;
		if (!FieldZoneRect(zone, &rect) || gameX < rect.x1 || gameX >= rect.x2 || gameY < rect.y1 || gameY >= rect.y2) {
			continue;
		}
		const float distance = std::fabs(gameX - (rect.x1 + rect.x2) * 0.5f);
		if (best < 0 || distance < bestDistance) {
			best = (int)zone;
			bestDistance = distance;
		}
	}
	return best;
}

enum class FieldState : uint8_t {
	UNSUPPORTED_GAME,
	MODULES_NOT_LOADED,
	READ_FAILED,
	DIALOG,
	OVERLAY_ACTIVE,
	NOT_READY,
	TRANSITION,
	HAND_PROMPT,
	UNKNOWN_PROMPT,
	INVALID_CURSOR,
	SETTLING,
	NORMAL,
	ATTACK_TARGET,
	CARD_TARGET,
};

inline const char *FieldStateName(FieldState state) {
	switch (state) {
	case FieldState::UNSUPPORTED_GAME: return "unsupported game";
	case FieldState::MODULES_NOT_LOADED: return "duel modules not loaded";
	case FieldState::READ_FAILED: return "read failed";
	case FieldState::DIALOG: return "dialog";
	case FieldState::OVERLAY_ACTIVE: return "overlay active";
	case FieldState::NOT_READY: return "not ready";
	case FieldState::TRANSITION: return "transition";
	case FieldState::HAND_PROMPT: return "hand-only prompt";
	case FieldState::UNKNOWN_PROMPT: return "unknown prompt";
	case FieldState::INVALID_CURSOR: return "invalid cursor";
	case FieldState::SETTLING: return "settling";
	case FieldState::NORMAL: return "normal";
	case FieldState::ATTACK_TARGET: return "attack target";
	case FieldState::CARD_TARGET: return "card target";
	}
	return "unknown";
}

struct FieldSnapshot {
	FieldState state = FieldState::UNSUPPORTED_GAME;
	uint32_t cursorCode = 0;
	uint32_t column = 0;
	uint32_t row = 0;
	uint32_t overlay = 0;
	uint32_t ready = 0;
	uint32_t promptFlags = 0;
	uint32_t selectAttack = 0;
	uint32_t selectCard = 0;
	uint32_t selectDialog = 0;
	uint32_t selectAnim = 0;
	float cursorX = 0.0f;
	// Raw card words for zones 0x00-0x13; valid from INVALID_CURSOR onward.
	uint32_t cards[FIELD_ZONES] = {};
	bool cardsValid = false;

	uint32_t OccupiedMask() const {
		uint32_t mask = 0;
		for (int i = 0; i < FIELD_ZONES; ++i) {
			if (cards[i] & FIELD_CARD_ID_MASK) {
				mask |= 1u << i;
			}
		}
		return mask;
	}
};

// Independent of the hand size, which the hand probe limits to 4-7 cards.
template <typename IsLoaded, typename ReadWord>
FieldSnapshot ObserveField(std::string_view discID, std::string_view discVersion, IsLoaded isLoaded, ReadWord readWord) {
	FieldSnapshot result;
	if (discID != "ULES01183" || discVersion != "1.02") {
		return result;
	}
	result.state = FieldState::MODULES_NOT_LOADED;
	if (!DuelModulesPresent(isLoaded, readWord)) {
		return result;
	}
	uint32_t cursorXBits = 0;
	uint32_t anim2 = 0;
	if (!readWord(CURSOR_X_ADDRESS, &cursorXBits) || !readWord(CURSOR_CODE_ADDRESS, &result.cursorCode) ||
		!readWord(CURSOR_COLUMN_ADDRESS, &result.column) || !readWord(CURSOR_ROW_ADDRESS, &result.row) ||
		!readWord(PROMPT_FLAGS_ADDRESS, &result.promptFlags) || !readWord(OVERLAY_ADDRESS, &result.overlay) ||
		!readWord(COMMAND_READY_ADDRESS, &result.ready) || !readWord(SELECT_ATTACK_ADDRESS, &result.selectAttack) ||
		!readWord(SELECT_CARD_ADDRESS, &result.selectCard) || !readWord(SELECT_DIALOG_ADDRESS, &result.selectDialog) ||
		!readWord(SELECT_ANIM_ADDRESS, &result.selectAnim) || !readWord(SELECT_ANIM2_ADDRESS, &anim2)) {
		result.state = FieldState::READ_FAILED;
		return result;
	}
	memcpy(&result.cursorX, &cursorXBits, sizeof(result.cursorX));
	for (uint32_t zone = 0; zone < (uint32_t)FIELD_ZONES; ++zone) {
		if (!readWord(FieldEntryAddress(zone), &result.cards[zone])) {
			result.state = FieldState::READ_FAILED;
			return result;
		}
	}
	result.cardsValid = true;

	const bool flagsValid = result.selectAttack <= 1 && result.selectCard <= 1;
	if (result.selectDialog != 0) {
		result.state = FieldState::DIALOG;
	} else if (result.overlay != OVERLAY_NONE) {
		result.state = FieldState::OVERLAY_ACTIVE;
	} else if (result.ready != 1) {
		result.state = FieldState::NOT_READY;
	} else if (!flagsValid || (result.selectAttack && result.selectCard)) {
		result.state = FieldState::UNKNOWN_PROMPT;
	} else if (anim2 != result.selectAnim) {
		// The two animation words differed while a prompt opened (1, then 3) and closed (4).
		result.state = FieldState::TRANSITION;
	} else if (result.selectAttack || result.selectCard) {
		if (result.promptFlags != PROMPT_SELECTION) {
			result.state = FieldState::UNKNOWN_PROMPT;
		} else if (result.selectAnim != SELECT_ANIM_OPEN) {
			result.state = FieldState::TRANSITION;
		} else {
			result.state = result.selectAttack ? FieldState::ATTACK_TARGET : FieldState::CARD_TARGET;
		}
	} else if (result.selectAnim != 0) {
		// A target choice closing: ready can already read 1 here.
		result.state = FieldState::TRANSITION;
	} else if (result.promptFlags == PROMPT_SELECTION) {
		// The hand-limit discard uses the prompt without the selection block. Hand only.
		result.state = FieldState::HAND_PROMPT;
	} else if (result.promptFlags != PROMPT_NORMAL) {
		result.state = FieldState::UNKNOWN_PROMPT;
	} else {
		result.state = FieldState::NORMAL;
	}
	if (result.state < FieldState::SETTLING) {
		return result;
	}

	// The game's own row and column words must agree with the zone map.
	const uint32_t zone = result.cursorCode & 0x3F;
	const uint32_t index = result.cursorCode >> 6;
	int row = -1;
	int column = -1;
	bool cursorValid = false;
	if (zone == PLAYER_HAND_ZONE) {
		cursorValid = result.row == ROW_PLAYER_HAND && std::isfinite(result.cursorX);
	} else if (zone == OPPONENT_HAND_ZONE) {
		cursorValid = result.row == ROW_OPPONENT_HAND;
	} else if (index == 0 && ZoneGridPosition(zone, &row, &column)) {
		cursorValid = result.row == (uint32_t)row && result.column == (uint32_t)column;
	}
	if (!cursorValid) {
		result.state = FieldState::INVALID_CURSOR;
	}
	return result;
}

inline bool IsFieldActionable(FieldState state) {
	return state == FieldState::NORMAL || state == FieldState::ATTACK_TARGET || state == FieldState::CARD_TARGET;
}

// Same rule as the hand gate: actionable only after STABLE_VBLANKS in one state with an unchanged
// board. Cursor moves don't restart it.
class FieldStabilityGate {
public:
	FieldSnapshot Apply(const FieldSnapshot &snapshot, uint64_t vblanks) {
		if (!IsFieldActionable(snapshot.state)) {
			Reset();
			return snapshot;
		}
		const uint32_t occupied = snapshot.OccupiedMask();
		if (!tracking_ || vblanks < start_ || snapshot.state != state_ || occupied != occupied_) {
			tracking_ = true;
			start_ = vblanks;
			state_ = snapshot.state;
			occupied_ = occupied;
		}
		if (vblanks - start_ >= STABLE_VBLANKS) {
			return snapshot;
		}
		FieldSnapshot settling = snapshot;
		settling.state = FieldState::SETTLING;
		return settling;
	}

	void Reset() {
		tracking_ = false;
	}

private:
	bool tracking_ = false;
	FieldState state_ = FieldState::UNSUPPORTED_GAME;
	uint64_t start_ = 0;
	uint32_t occupied_ = 0;
};

// With the cursor in the hand, the selected card is raised to y 201 and covers the bottom of the
// player's S/T row. Its center comes from the cursor's own X, so this holds for any hand size.
inline bool RaisedHandCardCovers(const FieldSnapshot &snapshot, float gameX, float gameY) {
	if ((snapshot.cursorCode & 0x3F) != PLAYER_HAND_ZONE || gameY < SELECTED_TOP) {
		return false;
	}
	if (!std::isfinite(snapshot.cursorX)) {
		return true;
	}
	return std::fabs(gameX - (WORLD_TO_PSP_X * snapshot.cursorX + HAND_CENTER_PSP_X)) <= SELECTED_HALF_WIDTH;
}

struct FieldHit {
	int zone = -1;          // Geometric zone, -1 outside the 20 tiles or under a hand card.
	bool occupied = false;
	uint32_t card = 0;
	bool accepted = false;  // Actionable state and a real zone. Nothing acts on this yet.
};

// handSlot is the hand probe's own result for the same point; the hand wins wherever it claims one.
inline FieldHit HitTestField(const FieldSnapshot &snapshot, int handSlot, float x, float y,
	float viewportX, float viewportY, float viewportW, float viewportH) {
	FieldHit hit;
	float gameX = 0.0f;
	float gameY = 0.0f;
	if (handSlot >= 0 || !PointerToGameFrame(x, y, viewportX, viewportY, viewportW, viewportH, &gameX, &gameY) ||
		RaisedHandCardCovers(snapshot, gameX, gameY)) {
		return hit;
	}
	hit.zone = FieldZoneAt(gameX, gameY);
	if (hit.zone < 0) {
		return hit;
	}
	if (snapshot.cardsValid) {
		hit.card = snapshot.cards[hit.zone];
		hit.occupied = (hit.card & FIELD_CARD_ID_MASK) != 0;
	}
	hit.accepted = IsFieldActionable(snapshot.state) && snapshot.cardsValid;
	return hit;
}

// Screen row of the cursor, with both hands; false for unknown zones.
inline bool CursorGridPosition(uint32_t cursorCode, int *row, int *column) {
	const uint32_t zone = cursorCode & 0x3F;
	if (zone == PLAYER_HAND_ZONE || zone == OPPONENT_HAND_ZONE) {
		*row = zone == PLAYER_HAND_ZONE ? ROW_PLAYER_HAND : ROW_OPPONENT_HAND;
		*column = -1;
		return true;
	}
	return ZoneGridPosition(zone, row, column);
}

enum class FieldButton : uint8_t { NONE, LEFT, RIGHT, UP, DOWN };

// One D-pad step toward a card zone, and where the game was seen to put the cursor for it. Rows wrap
// left/right through seven positions; columns 1-5 run straight through rows 1, 2, 4, 5. Leaving a
// hand lands on the column nearest the hand card, so only the row is predicted then. Returns false
// when the cursor is already on the target or a position is unknown.
struct FieldMove {
	FieldButton button = FieldButton::NONE;
	int row = -1;
	int column = -1;  // -1: any column.
};

inline bool PlanFieldMove(uint32_t cursorCode, uint32_t targetZone, FieldMove *move) {
	int row = 0;
	int column = 0;
	int targetRow = 0;
	int targetColumn = 0;
	if (!IsFieldCardZone(targetZone) || !ZoneGridPosition(targetZone, &targetRow, &targetColumn) ||
		!CursorGridPosition(cursorCode, &row, &column)) {
		return false;
	}
	if (row == ROW_PLAYER_HAND) {
		*move = { FieldButton::UP, 5, -1 };
		return true;
	}
	if (row == ROW_OPPONENT_HAND) {
		*move = { FieldButton::DOWN, 1, -1 };
		return true;
	}
	if (row == 3) {
		// The middle piles. Left from 0x1E was seen to jump rows, so leave vertically.
		*move = { FieldButton::DOWN, 4, column };
		return true;
	}
	if (column != targetColumn) {
		const int forward = (targetColumn - column + 7) % 7;
		const int backward = (column - targetColumn + 7) % 7;
		if (forward <= backward) {
			*move = { FieldButton::RIGHT, row, (column + 1) % 7 };
		} else {
			*move = { FieldButton::LEFT, row, (column + 6) % 7 };
		}
		return true;
	}
	if (row == targetRow) {
		return false;
	}
	static const int ROWS[4] = { 1, 2, 4, 5 };
	int index = 0;
	while (index < 4 && ROWS[index] != row) {
		++index;
	}
	if (index == 4) {
		return false;
	}
	if (targetRow < row) {
		*move = { FieldButton::UP, ROWS[index - 1], column };
	} else {
		*move = { FieldButton::DOWN, ROWS[index + 1], column };
	}
	return true;
}

inline bool FieldMoveMatches(const FieldMove &move, uint32_t cursorCode) {
	int row = 0;
	int column = 0;
	return CursorGridPosition(cursorCode, &row, &column) && row == move.row && (move.column < 0 || column == move.column);
}

// Moves the cursor to a tapped, occupied field card in normal play, one verified D-pad pulse at a
// time. Never presses Cross. Reads only gated snapshots: anything other than a settled normal board
// with the same cards, or a cursor that lands anywhere but the predicted zone, cancels and releases.
class FieldActionPlanner {
public:
	struct Context {
		FieldSnapshot snapshot;
		bool focused = false;
		bool paused = false;
		bool otherInput = false;
		uint32_t stateSerial = 0;
		uint64_t vblanks = 0;
		double wallTime = 0.0;
	};

	struct Step {
		FieldButton press = FieldButton::NONE;
		FieldButton release = FieldButton::NONE;
	};

	enum class TapResult : uint8_t { REJECTED, NAVIGATING, ALREADY_SELECTED };

	enum class CancelReason : uint8_t { NONE, REQUESTED, FOCUS, PAUSED, STATE_LOAD, CLOCK, OTHER_INPUT, TIMEOUT, STATE_CHANGED, BOARD_CHANGED, CURSOR, STEPS };

	TapResult Tap(int zone, const Context &context) {
		const FieldSnapshot &snapshot = context.snapshot;
		if (phase_ != Phase::IDLE || zone < 0 || !IsFieldCardZone((uint32_t)zone) || !Usable(context) ||
			snapshot.state != FieldState::NORMAL || !snapshot.cardsValid || !(snapshot.cards[zone] & FIELD_CARD_ID_MASK)) {
			return TapResult::REJECTED;
		}
		if ((snapshot.cursorCode & 0x3F) == (uint32_t)zone) {
			return TapResult::ALREADY_SELECTED;
		}
		FieldMove move;
		if (!PlanFieldMove(snapshot.cursorCode, (uint32_t)zone, &move)) {
			return TapResult::REJECTED;
		}
		target_ = zone;
		occupied_ = snapshot.OccupiedMask();
		stateSerial_ = context.stateSerial;
		lastVblanks_ = context.vblanks;
		lastCode_ = snapshot.cursorCode;
		deadline_ = context.wallTime + DEADLINE_SECONDS;
		steps_ = 0;
		waitUntil_ = context.vblanks;
		lastCancel_ = CancelReason::NONE;
		phase_ = Phase::READY;
		return TapResult::NAVIGATING;
	}

	Step Update(const Context &context) {
		if (phase_ == Phase::IDLE) {
			return {};
		}
		const CancelReason reason = Check(context);
		if (reason != CancelReason::NONE) {
			return Abort(reason);
		}
		lastVblanks_ = context.vblanks;
		const uint32_t code = context.snapshot.cursorCode;
		switch (phase_) {
		case Phase::READY: {
			if (code != lastCode_) {
				return Abort(CancelReason::CURSOR);
			}
			if (context.vblanks < waitUntil_) {
				return {};
			}
			if (!PlanFieldMove(code, (uint32_t)target_, &move_)) {
				// Arrived (checked again in WAIT), or nothing left that can be predicted.
				return (code & 0x3F) == (uint32_t)target_ ? Finish() : Abort(CancelReason::CURSOR);
			}
			if (++steps_ > MAX_STEPS) {
				return Abort(CancelReason::STEPS);
			}
			held_ = move_.button;
			waitUntil_ = context.vblanks + PRESS_VBLANKS;
			phase_ = Phase::PRESS;
			return { move_.button, FieldButton::NONE };
		}
		case Phase::PRESS:
			if (context.vblanks < waitUntil_) {
				return {};
			}
			held_ = FieldButton::NONE;
			waitUntil_ = context.vblanks + RESPONSE_VBLANKS;
			phase_ = Phase::WAIT;
			return { FieldButton::NONE, move_.button };
		case Phase::WAIT:
			if (code == lastCode_) {
				return context.vblanks >= waitUntil_ ? Abort(CancelReason::CURSOR) : Step{};
			}
			if (!FieldMoveMatches(move_, code)) {
				return Abort(CancelReason::CURSOR);
			}
			lastCode_ = code;
			if ((code & 0x3F) == (uint32_t)target_) {
				return Finish();
			}
			waitUntil_ = context.vblanks + GAP_VBLANKS;
			phase_ = Phase::READY;
			return {};
		default:
			return {};
		}
	}

	Step Cancel(CancelReason reason = CancelReason::REQUESTED) {
		return Abort(reason);
	}

	bool Active() const { return phase_ != Phase::IDLE; }
	CancelReason LastCancel() const { return lastCancel_; }
	int Target() const { return target_; }
	int Steps() const { return steps_; }

private:
	static constexpr uint64_t PRESS_VBLANKS = 5;
	static constexpr uint64_t GAP_VBLANKS = 2;
	static constexpr uint64_t RESPONSE_VBLANKS = 30;
	// Worst case seen in the model is 7 (hand to the far corner of the opponent's S/T row).
	static constexpr int MAX_STEPS = 12;
	static constexpr double DEADLINE_SECONDS = 3.0;

	enum class Phase : uint8_t { IDLE, READY, PRESS, WAIT };

	static bool Usable(const Context &context) {
		return context.focused && !context.paused && !context.otherInput;
	}

	CancelReason Check(const Context &context) const {
		if (!context.focused) {
			return CancelReason::FOCUS;
		}
		if (context.paused) {
			return CancelReason::PAUSED;
		}
		if (context.stateSerial != stateSerial_) {
			return CancelReason::STATE_LOAD;
		}
		if (context.vblanks < lastVblanks_) {
			return CancelReason::CLOCK;
		}
		if (context.otherInput) {
			return CancelReason::OTHER_INPUT;
		}
		if (context.wallTime > deadline_) {
			return CancelReason::TIMEOUT;
		}
		if (context.snapshot.state != FieldState::NORMAL || !context.snapshot.cardsValid) {
			return CancelReason::STATE_CHANGED;
		}
		if (context.snapshot.OccupiedMask() != occupied_) {
			return CancelReason::BOARD_CHANGED;
		}
		return CancelReason::NONE;
	}

	Step Finish() {
		phase_ = Phase::IDLE;
		return {};
	}

	Step Abort(CancelReason reason) {
		Step step;
		step.release = held_;
		held_ = FieldButton::NONE;
		if (phase_ != Phase::IDLE) {
			lastCancel_ = reason;
		}
		phase_ = Phase::IDLE;
		return step;
	}

	Phase phase_ = Phase::IDLE;
	FieldButton held_ = FieldButton::NONE;
	FieldMove move_;
	CancelReason lastCancel_ = CancelReason::NONE;
	int target_ = -1;
	int steps_ = 0;
	uint32_t occupied_ = 0;
	uint32_t stateSerial_ = 0;
	uint32_t lastCode_ = 0;
	uint64_t waitUntil_ = 0;
	uint64_t lastVblanks_ = 0;
	double deadline_ = 0.0;
};

}  // namespace TagForce3Duel
