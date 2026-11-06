// Copyright (c) 2012- PPSSPP Project.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0 or later.

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <deque>

// Converts a pointer position to the game's 480x272 frame. False outside the viewport, including
// black bars. Pointer and viewport must share the same pixel space.
inline bool PointerToGameFrame(float x, float y, float viewportX, float viewportY, float viewportW, float viewportH, float *gameX, float *gameY) {
	if (viewportW <= 0.0f || viewportH <= 0.0f) {
		return false;
	}
	*gameX = (x - viewportX) * 480.0f / viewportW;
	*gameY = (y - viewportY) * 272.0f / viewportH;
	return *gameX >= 0.0f && *gameX < 480.0f && *gameY >= 0.0f && *gameY < 272.0f;
}

// Moves a game's one-dimensional cursor to a target one verified press at a time. Pure: the caller
// supplies the observed index and maps directions to buttons. `now` is whatever clock the caller
// wants the pulses timed in.
enum class CursorDirection : uint8_t { NONE, PREVIOUS, NEXT };

struct CursorStep {
	CursorDirection press = CursorDirection::NONE;
	CursorDirection release = CursorDirection::NONE;
};

class CursorPlanner {
public:
	// Returns false for an invalid request. A cursor already on the target leaves the planner idle.
	bool Start(int current, int target, int count, bool wrap, int maxSteps, double now) {
		Cancel();
		if (count <= 0 || current < 0 || current >= count || target < 0 || target >= count) {
			return false;
		}
		current_ = current;
		target_ = target;
		count_ = count;
		wrap_ = wrap;
		maxSteps_ = maxSteps;
		nextPressAt_ = now;
		phase_ = current_ == target_ ? Phase::IDLE : Phase::READY;
		return true;
	}

	CursorStep Update(bool safe, int observed, double now) {
		if (phase_ == Phase::IDLE) {
			return {};
		}
		if (!safe) {
			return Cancel();
		}
		if (phase_ == Phase::PRESSING) {
			if (observed != current_ && observed != expected_) {
				return Cancel();
			}
			if (now < pressUntil_) {
				return {};
			}
			phase_ = Phase::WAITING;
			waitUntil_ = now + WAIT_SECONDS;
			CursorDirection released = held_;
			held_ = CursorDirection::NONE;
			return { CursorDirection::NONE, released };
		}
		if (phase_ == Phase::WAITING) {
			if (observed == expected_) {
				current_ = expected_;
				if (current_ == target_) {
					phase_ = Phase::IDLE;
					return {};
				}
				phase_ = Phase::READY;
				nextPressAt_ = now + GAP_SECONDS;
			} else if (observed != current_ || now > waitUntil_) {
				return Cancel();
			} else {
				return {};
			}
		}
		if (current_ == target_) {
			phase_ = Phase::IDLE;
			return {};
		}
		if (now < nextPressAt_) {
			return {};
		}
		if (observed != current_ || steps_ >= maxSteps_) {
			return Cancel();
		}
		held_ = NextDirection();
		const int delta = held_ == CursorDirection::NEXT ? 1 : -1;
		expected_ = wrap_ ? (current_ + delta + count_) % count_ : current_ + delta;
		pressUntil_ = now + PRESS_SECONDS;
		phase_ = Phase::PRESSING;
		++steps_;
		return { held_, CursorDirection::NONE };
	}

	// Changes destination mid-flight, keeping any press in progress. Fails once idle.
	bool Retarget(int target) {
		if (phase_ == Phase::IDLE || target < 0 || target >= count_) {
			return false;
		}
		target_ = target;
		steps_ = 0;
		return true;
	}

	CursorStep Cancel() {
		CursorStep step{ CursorDirection::NONE, held_ };
		held_ = CursorDirection::NONE;
		phase_ = Phase::IDLE;
		steps_ = 0;
		return step;
	}

	bool Active() const { return phase_ != Phase::IDLE; }
	int Steps() const { return steps_; }

private:
	static constexpr double PRESS_SECONDS = 0.08;
	static constexpr double GAP_SECONDS = 0.02;
	static constexpr double WAIT_SECONDS = 0.5;

	CursorDirection NextDirection() const {
		if (!wrap_) {
			return target_ > current_ ? CursorDirection::NEXT : CursorDirection::PREVIOUS;
		}
		const int forward = (target_ - current_ + count_) % count_;
		const int backward = (current_ - target_ + count_) % count_;
		return forward <= backward ? CursorDirection::NEXT : CursorDirection::PREVIOUS;
	}

	enum class Phase { IDLE, READY, PRESSING, WAITING };
	Phase phase_ = Phase::IDLE;
	CursorDirection held_ = CursorDirection::NONE;
	int current_ = 0;
	int target_ = 0;
	int expected_ = 0;
	int count_ = 0;
	int maxSteps_ = 0;
	int steps_ = 0;
	bool wrap_ = false;
	double pressUntil_ = 0.0;
	double waitUntil_ = 0.0;
	double nextPressAt_ = 0.0;
};

enum class TouchNavigationGesture {
	NONE,
	TAP,
	LEFT,
	RIGHT,
	UP,
	DOWN,
};

class TouchNavigation {
public:
	void PointerDown(int id, float x, float y, double now, bool mouse = false) {
		if (pointerId_ != -1) {
			CancelPointer();
			return;
		}
		pointerId_ = id;
		pointerMouse_ = mouse;
		downX_ = x;
		downY_ = y;
		maxDistanceSq_ = 0.0f;
		downTime_ = now;
	}

	void PointerMove(int id, float x, float y) {
		if (id == pointerId_) {
			float dx = x - downX_;
			float dy = y - downY_;
			maxDistanceSq_ = std::max(maxDistanceSq_, dx * dx + dy * dy);
		}
	}

	TouchNavigationGesture PointerUp(int id, float x, float y, double now) {
		if (id != pointerId_) {
			return TouchNavigationGesture::NONE;
		}
		PointerMove(id, x, y);
		pointerId_ = -1;
		float dx = x - downX_;
		float dy = y - downY_;
		if (maxDistanceSq_ <= 20.0f * 20.0f && now - downTime_ <= 0.35) {
			return TouchNavigationGesture::TAP;
		}
		if (dx * dx + dy * dy < 50.0f * 50.0f) {
			return TouchNavigationGesture::NONE;
		}
		if (std::abs(dx) >= std::abs(dy)) {
			return dx < 0.0f ? TouchNavigationGesture::LEFT : TouchNavigationGesture::RIGHT;
		}
		return dy < 0.0f ? TouchNavigationGesture::UP : TouchNavigationGesture::DOWN;
	}

	void CancelPointer() { pointerId_ = -1; }
	bool HasPointer() const { return pointerId_ != -1; }
	bool PointerIsMouse() const { return pointerMouse_; }
	int PointerId() const { return pointerId_; }

	void Queue(uint32_t button) {
		if (button && pending_.size() < 16) {
			pending_.push_back(button);
		}
	}

	struct Step {
		uint32_t press = 0;
		uint32_t release = 0;
	};

	Step Update(double now) {
		if (activeButton_ && now >= releaseTime_) {
			Step step{ 0, activeButton_ };
			activeButton_ = 0;
			nextPressTime_ = now + 0.02;
			return step;
		}
		if (!activeButton_ && !pending_.empty() && now >= nextPressTime_) {
			activeButton_ = pending_.front();
			pending_.pop_front();
			releaseTime_ = now + 0.08;
			return Step{ activeButton_, 0 };
		}
		return {};
	}

	uint32_t Reset() {
		CancelPointer();
		pending_.clear();
		uint32_t active = activeButton_;
		activeButton_ = 0;
		nextPressTime_ = 0.0;
		return active;
	}

private:
	int pointerId_ = -1;
	bool pointerMouse_ = false;
	float downX_ = 0.0f;
	float downY_ = 0.0f;
	float maxDistanceSq_ = 0.0f;
	double downTime_ = 0.0;
	std::deque<uint32_t> pending_;
	uint32_t activeButton_ = 0;
	double releaseTime_ = 0.0;
	double nextPressTime_ = 0.0;
};
