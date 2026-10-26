#pragma once

// HudLogCapture — runtime-owned recent-activity capture for the HUD
// (HudFrameData::activityLog, source runtime.log.recent).
//
// An openFrameworks logger channel that forwards every message unchanged to
// the channel it replaced (so the console prints exactly as before) and
// keeps the last kMaxLines as "Module: message" — no level tag. Warnings
// and errors keep a short "WARN " / "ERR " marker so they stay
// distinguishable in a tag-free log. Thread-safe: decoder and loader
// threads log too.

#include "ofLog.h"

#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

class HudLogCapture : public ofBaseLoggerChannel {
public:
	static constexpr size_t kMaxLines = 12;

	explicit HudLogCapture(std::shared_ptr<ofBaseLoggerChannel> forwardTo) : forward_(std::move(forwardTo)) {}

	void log(ofLogLevel level, const std::string& module, const std::string& message) override {
		if (forward_) forward_->log(level, module, message);
		std::string line;
		if (level == OF_LOG_WARNING) line = "WARN ";
		else if (level >= OF_LOG_ERROR) line = "ERR ";
		line += module.empty() ? message : module + ": " + message;
		for (char& c : line) if (c == '\n' || c == '\r' || c == '\t') c = ' ';
		std::lock_guard<std::mutex> lock(mutex_);
		lines_.push_back(std::move(line));
		while (lines_.size() > kMaxLines) lines_.pop_front();
	}

	// Newest last.
	std::vector<std::string> recent() const {
		std::lock_guard<std::mutex> lock(mutex_);
		return std::vector<std::string>(lines_.begin(), lines_.end());
	}

private:
	std::shared_ptr<ofBaseLoggerChannel> forward_;
	mutable std::mutex mutex_;
	std::deque<std::string> lines_;
};
