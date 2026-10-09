#include "HudFocusNavigator.h"

#include <cstdlib>

int HudFocusNavigator::indexOf(const std::string& id) const {
	for (size_t i = 0; i < slots_.size(); ++i) if (slots_[i] == id) return int(i);
	return -1;
}

void HudFocusNavigator::setFocus(int index) {
	if (index < 0) {
		focused_.reset();
		focusedAvailability_ = HudControlAvailability::Hidden;
		return;
	}
	focused_ = slots_[size_t(index)];
	focusedAvailability_ = availability_[size_t(index)];
}

void HudFocusNavigator::refresh(const AvailabilityFn& availability) {
	availability_.assign(slots_.size(), HudControlAvailability::Hidden);
	for (size_t i = 0; i < slots_.size(); ++i) availability_[i] = availability(slots_[i]);
	if (!engaged_ || !focused_) return;

	const int current = indexOf(*focused_);
	if (current >= 0 && visible(size_t(current))) {
		focusedAvailability_ = availability_[size_t(current)]; // same control, availability may change
		return;
	}
	// Focused control disappeared: next visible after it, else previous, else none.
	const int n = int(slots_.size());
	for (int i = current + 1; i < n; ++i) if (visible(size_t(i))) { setFocus(i); return; }
	for (int i = current - 1; i >= 0; --i) if (visible(size_t(i))) { setFocus(i); return; }
	setFocus(-1);
}

bool HudFocusNavigator::step(int delta) {
	std::vector<int> visibleIdx;
	for (size_t i = 0; i < slots_.size(); ++i) if (visible(i)) visibleIdx.push_back(int(i));
	if (visibleIdx.empty()) return false;

	if (!engaged_ || !focused_) {
		engaged_ = true;
		for (int i : visibleIdx) {
			if (availability_[size_t(i)] == HudControlAvailability::Enabled) { setFocus(i); return true; }
		}
		setFocus(visibleIdx.front());
		return true;
	}

	int pos = 0;
	const int current = indexOf(*focused_);
	for (size_t k = 0; k < visibleIdx.size(); ++k) if (visibleIdx[k] == current) pos = int(k);
	const int count = int(visibleIdx.size());
	const int next = ((pos + delta) % count + count) % count;
	setFocus(visibleIdx[size_t(next)]);
	return true;
}

std::optional<std::string> HudFocusNavigator::activationTarget() const {
	if (focused_ && focusedAvailability_ == HudControlAvailability::Enabled) return focused_;
	return std::nullopt;
}
