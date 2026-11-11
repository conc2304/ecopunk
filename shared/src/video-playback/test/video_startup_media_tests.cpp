// OC-003 startup-media verification control (VideoPlaybackService::Config::verificationStartupMediaId).
// Real VideoPlaybackService with a fake IVideoDecoder and a temp media root of empty .mp4 files (same harness as
// video_activation_count_tests.cpp). OF-linked; no window or GL context is created.

#include "../VideoPlaybackService.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace {
int g_total = 0, g_failures = 0;
#define CHECK(cond, what) do { g_total++; if (!(cond)) { g_failures++; std::cerr << "FAIL: " << (what) << "  (" << __LINE__ << ")\n"; } } while (0)

struct FakeControl {
	bool failAll = false;       // every load() fails
	std::string failName;       // load() fails for a path containing this
	int loads = 0;
};

class FakeDecoder : public IVideoDecoder {
public:
	explicit FakeDecoder(FakeControl* c) : ctl_(c) {}
	bool load(const std::string& path) override {
		ctl_->loads++;
		loaded_ = false;
		if (ctl_->failAll) return false;
		if (!ctl_->failName.empty() && path.find(ctl_->failName) != std::string::npos) return false;
		loaded_ = true;
		return true;
	}
	void update(float) override {}
	void close() override { loaded_ = false; }
	bool isLoaded() const override { return loaded_; }
	bool isFrameNew() const override { return false; }
	float getPosition() const override { return loaded_ ? 0.0f : -1.0f; }
	float getDuration() const override { return loaded_ ? 10.0f : 0.0f; }
	bool seekToSeconds(float) override { return loaded_; }
	glm::ivec2 getSize() const override { return glm::ivec2(0, 0); }
	const ofTexture* getTexture() const override { return nullptr; }
	const ofPixels* getPixels() const override { return nullptr; }
private:
	FakeControl* ctl_;
	bool loaded_ = false;
};

std::string makeRoot(const char* tag, int files) {
	const auto dir = std::filesystem::temp_directory_path() / (std::string("oc003_startup_media_") + tag);
	std::filesystem::remove_all(dir);
	std::filesystem::create_directories(dir);
	for (int i = 0; i < files; ++i) std::ofstream(dir / (std::string("clip") + char('a' + i) + ".mp4")).put('x');
	return dir.string();
}

VideoPlaybackService::Config configFor(const std::string& root) {
	VideoPlaybackService::Config c;
	c.mediaRoot = root;
	c.automaticAdvance = false; // media changes only when the test asks
	return c;
}
} // namespace

int main() {
	// Learn the canonical ids of a 4-clip root from a normal (random) startup + navigation.
	const std::string root = makeRoot("four", 4);
	std::vector<std::string> ids;
	{
		FakeControl ctl;
		VideoPlaybackService svc(std::make_unique<FakeDecoder>(&ctl));
		VideoPlaybackService::Config c = configFor(root);
		CHECK(svc.setup(c), "default setup");
		for (int i = 0; i < 4; ++i) { ids.push_back(svc.status().mediaId.value_or("")); svc.next(); }
	}
	CHECK(ids.size() == 4, "collected four ids");

	// -- deterministic: every requested id wins 25 times out of 25 (random startup would not) ----
	for (const std::string& want : ids) {
		int hits = 0;
		for (int run = 0; run < 25; ++run) {
			FakeControl ctl;
			VideoPlaybackService svc(std::make_unique<FakeDecoder>(&ctl));
			VideoPlaybackService::Config c = configFor(root);
			c.verificationStartupMediaId = want;
			svc.setup(c);
			if (svc.status().health == VideoPlaybackHealth::Ready && svc.status().mediaId.value_or("") == want && svc.activationCount() == 1) ++hits;
		}
		CHECK(hits == 25, "requested media is the startup item in every run: " + want);
	}

	// -- origin stays Startup, a later next() still goes through the normal policy ---------------
	{
		FakeControl ctl;
		VideoPlaybackService svc(std::make_unique<FakeDecoder>(&ctl));
		VideoPlaybackService::Config c = configFor(root);
		c.verificationStartupMediaId = ids[2];
		svc.setup(c);
		CHECK(svc.status().mediaId.value_or("") == ids[2], "pinned startup");
		CHECK(svc.next(), "next() after a pinned startup works");
		CHECK(svc.status().mediaId.value_or("") != ids[2], "and moves to a different clip");
	}

	// -- invalid id: rejected visibly, nothing loaded, no fallback ---------------------------------
	for (const char* bad : { "media.does-not-exist", "list", "  " }) {
		FakeControl ctl;
		VideoPlaybackService svc(std::make_unique<FakeDecoder>(&ctl));
		VideoPlaybackService::Config c = configFor(root);
		c.verificationStartupMediaId = bad;
		CHECK(svc.setup(c), "setup itself still succeeds");
		const VideoPlaybackStatus st = svc.status();
		CHECK(st.health == VideoPlaybackHealth::Failed, std::string("invalid id -> Failed: ") + bad);
		CHECK(!st.mediaId.has_value(), std::string("no media selected: ") + bad);
		CHECK(svc.activationCount() == 0, std::string("no activation: ") + bad);
		CHECK(ctl.loads == 0, std::string("decoder never asked to load anything: ") + bad);
		CHECK(st.messageId.value_or("") == "media.error.verification_media_unavailable", std::string("visible error id: ") + bad);
	}

	// -- valid id whose file fails to load: rejected, NOT replaced by another clip -------------------
	{
		FakeControl ctl;
		VideoPlaybackService svc(std::make_unique<FakeDecoder>(&ctl));
		// find which file belongs to ids[1]: load it normally first to learn the path name
		VideoPlaybackService probe(std::make_unique<FakeDecoder>(&ctl));
		VideoPlaybackService::Config pc = configFor(root);
		pc.verificationStartupMediaId = ids[1];
		probe.setup(pc);
		const std::string path = probe.currentAbsolutePath().value_or("");
		CHECK(!path.empty(), "learned the path of the requested clip");
		ctl.loads = 0;
		ctl.failName = std::filesystem::path(path).filename().string();
		VideoPlaybackService::Config c = configFor(root);
		c.verificationStartupMediaId = ids[1];
		svc.setup(c);
		CHECK(svc.status().health == VideoPlaybackHealth::Failed, "unloadable requested media -> Failed");
		CHECK(ctl.loads == 1, "exactly one load attempt: no Recovery fallback to another clip");
		CHECK(!svc.status().mediaId.has_value(), "no other media became active");
		CHECK(svc.activationCount() == 0, "no activation counted");
	}

	// -- default behaviour unchanged: unset -> normal startup, any of the catalog, Ready -------------
	{
		std::set<std::string> seen;
		for (int run = 0; run < 60; ++run) {
			FakeControl ctl;
			VideoPlaybackService svc(std::make_unique<FakeDecoder>(&ctl));
			svc.setup(configFor(root));
			CHECK(svc.status().health == VideoPlaybackHealth::Ready && svc.activationCount() == 1, "default startup Ready, one activation");
			seen.insert(svc.status().mediaId.value_or(""));
		}
		CHECK(seen.size() > 1, "unset control keeps the random startup (more than one distinct first clip over 60 runs)");
	}

	std::cout << (g_total - g_failures) << "/" << g_total << " checks passed\n";
	return g_failures == 0 ? 0 : 1;
}
