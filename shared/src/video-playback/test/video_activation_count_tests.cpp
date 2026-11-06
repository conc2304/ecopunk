// OC-003B: VideoPlaybackService::activationCount() — one bump per SUCCESSFUL
// media activation (including same-media reactivation), none for a failed load,
// none for ordinary updates. Uses the real service with a fake IVideoDecoder and
// a temp media root of empty .mp4 files (the fake never reads them). OF-linked
// (ofDirectory/ofFilePath/ofLog); no window or GL context is created.

#include "../VideoPlaybackService.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>

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
	const auto dir = std::filesystem::temp_directory_path() / (std::string("oc003b_activation_") + tag);
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
	// -- first activation + ordinary updates + media change -----------------------
	{
		FakeControl ctl;
		VideoPlaybackService svc(std::make_unique<FakeDecoder>(&ctl));
		CHECK(svc.activationCount() == 0, "count is 0 before setup");
		CHECK(svc.setup(configFor(makeRoot("two", 2))), "setup with two clips");
		CHECK(svc.status().health == VideoPlaybackHealth::Ready, "startup reaches Ready");
		CHECK(svc.activationCount() == 1, "first activation bumps to 1");
		const std::string first = svc.status().mediaId.value_or("");

		for (int i = 0; i < 500; ++i) svc.update(1.0f / 60.0f);
		CHECK(svc.activationCount() == 1, "500 ordinary updates leave the count unchanged");
		for (int i = 0; i < 100; ++i) { (void)svc.status(); (void)svc.currentTexture(); (void)svc.isFrameNew(); }
		CHECK(svc.activationCount() == 1, "status()/currentTexture()/isFrameNew() reads never bump it");

		CHECK(svc.next(), "next() succeeds");
		CHECK(svc.activationCount() == 2, "media change bumps once (to 2)");
		CHECK(svc.status().mediaId.value_or("") != first, "the media actually changed");

		CHECK(svc.previous(), "previous() succeeds");
		CHECK(svc.activationCount() == 3, "returning to the first media bumps again (revisit, same mediaId as before)");
		CHECK(svc.status().mediaId.value_or("") == first, "revisit has the original mediaId");
	}

	// -- same-media reactivation (single-item catalog, boundary advance) ----------
	{
		FakeControl ctl;
		VideoPlaybackService svc(std::make_unique<FakeDecoder>(&ctl));
		CHECK(svc.setup(configFor(makeRoot("one", 1))), "setup with one clip");
		CHECK(svc.activationCount() == 1, "single clip: first activation = 1");
		const std::string id = svc.status().mediaId.value_or("");
		const bool advanced = svc.advanceAtBoundary();
		CHECK(advanced, "boundary advance on a single clip reactivates it");
		CHECK(svc.status().mediaId.value_or("") == id, "same mediaId after reactivation");
		CHECK(svc.activationCount() == 2, "same-media reactivation bumps the count");
		svc.advanceAtBoundary();
		CHECK(svc.activationCount() == 3, "and again on the next reactivation");
	}

	// -- failed load ---------------------------------------------------------------
	{
		FakeControl ctl;
		VideoPlaybackService svc(std::make_unique<FakeDecoder>(&ctl));
		CHECK(svc.setup(configFor(makeRoot("fail", 2))), "setup");
		CHECK(svc.activationCount() == 1, "startup = 1");
		ctl.failAll = true;
		const int loadsBefore = ctl.loads;
		const bool ok = svc.next();
		CHECK(!ok && svc.status().health == VideoPlaybackHealth::Failed, "all candidates failing leaves the service Failed");
		CHECK(ctl.loads > loadsBefore, "loads were attempted");
		CHECK(svc.activationCount() == 1, "failed activation does not bump the count");
		for (int i = 0; i < 60; ++i) svc.update(1.0f / 60.0f);
		CHECK(svc.activationCount() == 1, "updates while Failed do not bump it");
		ctl.failAll = false;
		CHECK(svc.next() && svc.status().health == VideoPlaybackHealth::Ready, "recovers when loads work again");
		CHECK(svc.activationCount() == 2, "the recovery activation bumps once");
	}

	// -- one candidate fails, a fallback succeeds: exactly one bump -----------------
	{
		FakeControl ctl;
		VideoPlaybackService svc(std::make_unique<FakeDecoder>(&ctl));
		CHECK(svc.setup(configFor(makeRoot("partial", 3))), "setup with three clips");
		const std::string active = svc.status().displayFileName.value_or("");
		// Fail one clip that is NOT the active one; the third remains a working fallback.
		ctl.failName = active == "clipa.mp4" ? "clipb" : "clipa";
		const uint64_t before = svc.activationCount();
		svc.next();
		CHECK(svc.status().health == VideoPlaybackHealth::Ready, "a working candidate activates");
		CHECK(svc.status().displayFileName.value_or("") != ctl.failName + ".mp4", "the failing clip is not the active one");
		CHECK(svc.activationCount() == before + 1, "a failed try (if any) plus one successful activation bumps exactly once");
	}

	// -- two clips, the other one fails: nothing to recover onto, no bump -----------
	{
		FakeControl ctl;
		VideoPlaybackService svc(std::make_unique<FakeDecoder>(&ctl));
		CHECK(svc.setup(configFor(makeRoot("pair", 2))), "setup with two clips");
		const std::string active = svc.status().displayFileName.value_or("");
		ctl.failName = active == "clipa.mp4" ? "clipb" : "clipa";
		const uint64_t before = svc.activationCount();
		svc.next();
		CHECK(svc.status().health == VideoPlaybackHealth::Failed, "existing behaviour: no fallback to the outgoing clip, so Failed");
		CHECK(svc.activationCount() == before, "so the failed attempt leaves the count unchanged");
	}

	std::cout << (g_total - g_failures) << "/" << g_total << " checks passed\n";
	return g_failures ? 1 : 0;
}
