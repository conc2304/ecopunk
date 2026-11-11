#pragma once

// HudRegionInteraction — the input state machine of ONE interactive HUD region (a "selector
// group": N stacked option rows). OF-free, covered by test/hud_region_interaction_tests.cpp.
//
// Owns ONLY "is the region being edited, and which row is selected". It never knows what a row
// means and never touches a scene: a value step is reported to the caller, which forwards it
// through its own authorised path (ExperienceRuntime's region-option port).
//
//   FOCUSED --activate--> ACTIVE (row = last row, default 0)
//   ACTIVE:  Up        -> previous row (clamped: Up on the first row stays)
//            Down      -> next row; Down on the LAST row leaves editing (moveDownOrExit)
//            Left/Right-> stepValue(delta)       (the CALLER wraps the option value)
//            activate/back -> exit, region stays focused; applied values are NOT reverted
//   scene change / region unavailable -> forceExit()  (no stale editing state)
//
// Encoder fallback (one axis + a click): rotation = stepValue on the selected row; click =
// advanceRowOrExit() (PIXELS -> PALETTE -> exit) so a pure encoder can never get trapped.
// Keyboard Enter uses exit() directly (spec: Enter/Escape leave).

class HudRegionInteraction {
public:
	explicit HudRegionInteraction(int rows = 2) : rows_(rows < 0 ? 0 : rows) {}

	// The row count may change at any time (a scene mode with fewer controls). 0 rows = nothing to
	// edit: the region cannot be entered and an active session is closed (never a dead-end focus).
	void configure(int rows) {
		rows_ = rows < 0 ? 0 : rows;
		if (rows_ == 0) { active_ = false; row_ = 0; return; }
		if (row_ >= rows_) row_ = rows_ - 1; // a stale row index is repaired, never kept against a shorter list
	}

	bool active() const { return active_; }
	int row() const { return row_; }
	int rows() const { return rows_; }

	// Enters editing. The previously selected row is kept (default row 0).
	bool enter() {
		if (active_ || rows_ < 1) return false;
		active_ = true;
		return true;
	}
	// Leaves editing; the region keeps HUD focus. Values already applied stay applied.
	bool exit() {
		if (!active_) return false;
		active_ = false;
		return true;
	}
	void forceExit() { active_ = false; row_ = 0; }

	// Up/Down: clamped (documented choice) — never wraps.
	bool selectRow(int delta) {
		if (!active_) return false;
		const int next = row_ + delta;
		const int clamped = next < 0 ? 0 : (next >= rows_ ? rows_ - 1 : next);
		const bool moved = clamped != row_;
		row_ = clamped;
		return moved;
	}

	// Down: next row; on the LAST row it LEAVES editing (required: no dead end, also for 1-row groups).
	// Returns true if it left editing.
	bool moveDownOrExit() {
		if (!active_) return false;
		if (row_ + 1 >= rows_) { active_ = false; return true; }
		++row_;
		return false;
	}

	// Encoder click: next row; after the last row, leave editing.
	// Returns true if it left editing.
	bool advanceRowOrExit() {
		if (!active_) return false;
		if (row_ + 1 >= rows_) { active_ = false; return true; }
		++row_;
		return false;
	}

private:
	int rows_ = 2;
	int row_ = 0;
	bool active_ = false;
};
