// Copyright (c) 2012- PPSSPP Project.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0 or later.

#pragma once

#include <cstdint>
#include <string_view>

#include "UI/TouchNavigation.h"

namespace TagForce3Language {

constexpr uint32_t SELECTION_ADDRESS = 0x08EEACD9;
// Not a screen ID: this word climbs as the game progresses (7 also occurs in a duel intro, and it
// reaches 8-13 inside one duel). It only narrows the check when modtitle is the loaded module.
constexpr uint32_t BOOT_PROGRESS_ADDRESS = 0x089218B4;
constexpr uint32_t LANGUAGE_SETTINGS_PROGRESS = 7;
constexpr uint32_t MODTITLE_BASE = 0x08E9AA00;

struct Snapshot {
	bool supportedGame = false;
	bool validRead = false;
	bool matchesObservedPattern = false;
	uint8_t selection = 0;
};

// Measured from ULES01183 v1.02 Language Settings screenshots. Coordinates are
// in the game's 480x272 framebuffer; gaps between rows remain inactive.
inline int HitTest(float x, float y, float viewportX, float viewportY, float viewportW, float viewportH) {
	float gameX = 0.0f;
	float gameY = 0.0f;
	if (!PointerToGameFrame(x, y, viewportX, viewportY, viewportW, viewportH, &gameX, &gameY)) {
		return -1;
	}
	if (gameX < 140.0f || gameX >= 340.0f) {
		return -1;
	}
	for (int i = 0; i < 5; ++i) {
		float top = 128.0f + i * 26.0f;
		if (gameY >= top && gameY < top + 22.0f) {
			return i;
		}
	}
	return -1;
}

// The progress word is in ELF/modehsys; the menu bytes are in ELF/modtitle, and are stale whenever
// that module isn't loaded, so it must be active at its measured base before they mean anything.
template <typename IsLoaded, typename ReadByte, typename ReadWord>
Snapshot Observe(std::string_view discID, std::string_view discVersion, IsLoaded isLoaded, ReadByte readByte, ReadWord readWord) {
	Snapshot result;
	result.supportedGame = discID == "ULES01183" && discVersion == "1.02";
	if (!result.supportedGame) {
		return result;
	}
	uint32_t moduleBase = 0;
	if (!isLoaded("modtitle", &moduleBase) || moduleBase != MODTITLE_BASE) {
		return result;
	}
	uint32_t progress = 0;
	if (!readWord(BOOT_PROGRESS_ADDRESS, &progress) || progress != LANGUAGE_SETTINGS_PROGRESS) {
		return result;
	}
	uint8_t menuMode = 0;
	uint8_t selection = 0;
	uint8_t fieldDC = 0;
	uint8_t fieldDE = 0;
	if (!readByte(0x08EEACD2, &menuMode) || !readByte(SELECTION_ADDRESS, &selection) ||
		!readByte(0x08EEACDC, &fieldDC) || !readByte(0x08EEACDE, &fieldDE)) {
		return result;
	}
	result.validRead = true;
	result.selection = selection;
	result.matchesObservedPattern = menuMode == 7 && fieldDC == 1 && fieldDE == 1 && selection <= 4;
	return result;
}

enum class Direction : uint8_t { NONE, UP, DOWN };

struct Step {
	Direction press = Direction::NONE;
	Direction release = Direction::NONE;
};

struct Context {
	Snapshot snapshot;
	bool confirmedMenu = false;
	bool focused = false;
	bool paused = false;
	uint32_t stateLoadSerial = 0;
};

// Pure planner. A caller must independently validate menu identity before setting confirmedMenu.
class Planner {
public:
	bool Start(int target, const Context &context, double now) {
		Cancel();
		if (!Safe(context) || target < 0 || target > 4 || context.snapshot.selection > 4) {
			return false;
		}
		loadSerial_ = context.stateLoadSerial;
		deadline_ = now + 2.0;
		return cursor_.Start(context.snapshot.selection, target, 5, false, 4, now);
	}

	Step Update(const Context &context, double now) {
		if (!cursor_.Active()) {
			return {};
		}
		const bool safe = Safe(context) && context.stateLoadSerial == loadSerial_ && now <= deadline_;
		return ToStep(cursor_.Update(safe, context.snapshot.selection, now));
	}

	Step Cancel() { return ToStep(cursor_.Cancel()); }

	bool Active() const { return cursor_.Active(); }
	int Steps() const { return cursor_.Steps(); }

private:
	static bool Safe(const Context &context) {
		return context.snapshot.supportedGame && context.snapshot.validRead &&
			context.snapshot.matchesObservedPattern && context.confirmedMenu &&
			context.focused && !context.paused;
	}

	static Direction ToDirection(CursorDirection direction) {
		return direction == CursorDirection::NEXT ? Direction::DOWN :
			direction == CursorDirection::PREVIOUS ? Direction::UP : Direction::NONE;
	}

	static Step ToStep(CursorStep step) {
		return { ToDirection(step.press), ToDirection(step.release) };
	}

	CursorPlanner cursor_;
	uint32_t loadSerial_ = 0;
	double deadline_ = 0.0;
};

}  // namespace TagForce3Language
