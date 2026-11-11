#include <chrono>
#include <sstream>
#include <filesystem>
#include <fstream>
#include "ExperienceRuntime.h"
#include "ExperienceSettingsPageView.h"
#include "Rt004StallProbe.h" // RT-004 diagnostic probe (env-gated, no behavior change)

#include "SceneRenderGuard.h"

#include "ofAppRunner.h"
#include "ofFileUtils.h"
#include "ofGraphics.h"
#include "ofLog.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib> // std::getenv (EXPERIENCE_RUNTIME_HUD_SKIN)
#include <ctime>   // localtime_r (day/night skin schedule)

ExperienceRuntime::ExperienceRuntime()
	: blobScene_(runtimeServices_.video())
	, temporalScene_(runtimeServices_.video())
	, blueprintScene_(runtimeServices_.video())
	, fragmentScene_(runtimeServices_.video())
	, quadrantScene_(runtimeServices_.video())
	, crtPhosphorScene_(runtimeServices_.video()) {
	regionOptionSink_ = [this](int row, int delta) { return sceneManager_.dispatchSceneOption(row, delta); };
	// See ExperienceRuntime.h's constructor comment and blobScene_'s own
	// declaration comment: runtimeServices_.video() is a valid, stable
	// reference from the moment runtimeServices_ itself is constructed
	// (declaration order, above blobScene_/temporalScene_ in the header),
	// independent of whether RuntimeServices::setup() has run yet — that
	// happens later, in setup() below, before either scene is activated.
}

void ExperienceRuntime::installProductionScenes(const std::string& startupSceneId, ProductionRoster roster) {
	if (!productionScenesInstalled_) {
		// Fixed registry order = NextScene ring order: Blob, Temporal, Blueprint, Fragment, Quadrant, (Production roster only) CrtPhosphor.
		sceneManager_.registerProductionScene(&blobScene_); // no canonical effect producer -> effects = nullopt
		sceneManager_.registerProductionScene(
			&temporalScene_, [this] { return temporalScene_.currentEffectActivitySnapshot(); });
		if (roster != ProductionRoster::BlobTemporal) {
			sceneManager_.registerProductionScene(&blueprintScene_); // no canonical effect producer -> effects = nullopt
			if (BlueprintProductionScene::kMediaAdvancesAtCycleBoundary) {
				boundaryDrivenMediaScenes_.push_back(BlueprintProductionScene::kSceneId);
			}
		}
		if (roster == ProductionRoster::Full || roster == ProductionRoster::Production) {
			// Neither has a canonical effect producer -> effects = nullopt.
			// Both use timer-driven Shared Video cadence (not boundary-driven),
			// like Blob/Temporal.
			sceneManager_.registerProductionScene(&fragmentScene_);
			sceneManager_.registerProductionScene(&quadrantScene_);
		}
		if (roster == ProductionRoster::Production) {
			// Sixth scene, normal run only (harness rosters unchanged). No canonical
			// effect producer -> effects = nullopt; timer-driven Shared Video cadence.
			sceneManager_.registerProductionScene(&crtPhosphorScene_);
		}
		productionScenesInstalled_ = true;
	}
	sceneManager_.setStartupScene(startupSceneId);
}

void ExperienceRuntime::applyMediaCadence(bool liveThisFrame) {
	const std::string& owner = sceneManager_.status().activeSceneId;
	const bool boundaryDriven = liveThisFrame
		&& std::find(boundaryDrivenMediaScenes_.begin(), boundaryDrivenMediaScenes_.end(), owner) != boundaryDrivenMediaScenes_.end();
	// Installation mediaAutoCycle: the setting gates Shared Video's own
	// automatic-advance timer (still the only media scheduler); boundary-
	// driven scenes keep it off as before.
	const installation::AutoCycle& media = installation_.settings().mediaAutoCycle;
	const bool timerWanted = media.enabled && !boundaryDriven && !settingsPage_.isOpen(); // paused while Settings is open; re-enabling starts a fresh dwell
	VideoPlaybackService& video = runtimeServices_.video();
	if (video.minimumDwellSeconds() != media.durationSeconds) video.setMinimumDwellSeconds(media.durationSeconds);
	if (video.automaticAdvanceEnabled() != timerWanted) {
		video.setAutomaticAdvanceEnabled(timerWanted);
		ofLogNotice("ExperienceRuntime") << "media cadence -> "
			<< (boundaryDriven ? "scene-boundary-driven (" + owner + ")" : timerWanted ? std::string("automatic") : std::string("manual only (installation setting)"));
	}
}

namespace {
// Monotonic seconds: Settings-page timing (OK debounce, OS-clock re-read
// cadence) must not move when the wall clock is changed.
double steadySeconds() {
	return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
} // namespace

// -- Experience Settings page v0.1 -------------------------------------------
void ExperienceRuntime::refreshSettingsContext(bool rereadOsClock) {
	const double now = steadySeconds();
	if (rereadOsClock || now - clockSnapshotAt_ > 10.0) {
		clockSnapshot_ = clockService_.read(); // one fixed-argv `timedatectl show` on Linux
		clockSnapshotAt_ = now;
	}
	settingspage::Context c;
	c.settings = installation_.settings();
	c.persistenceEnabled = !installationSettingsPath_.empty();
	c.persistenceStatus = saveStatus_;
	c.persistenceFailed = saveFailed_;
	c.clock = clockSnapshot_;
	// The seconds tick live without re-running timedatectl.
	const std::time_t t = std::time(nullptr);
	std::tm lt{};
	if (localtime_r(&t, &lt) != nullptr) {
		c.clock.local = {lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday, lt.tm_hour, lt.tm_min};
		c.clock.second = lt.tm_sec;
	}
	const systemclock::WriteCapability cap = clockService_.writeCapability();
	c.clockWritable = cap.enabled;
	c.clockWriteNote = cap.note;
	c.clockResult = clockResult_;
	c.clockResultFailed = clockResultFailed_;
	c.modeOptions = minimaltheme::modeOptions(skinSchedule_.valid() ? skinSchedule_.allSkins() : std::vector<std::string>{}, themes_);
	for (const auto& b : themes_.light.backgrounds) c.lightBackgrounds.push_back(b.name);
	for (const auto& b : themes_.dark.backgrounds) c.darkBackgrounds.push_back(b.name);
	c.zones = &clockService_.zones();
	c.commonZones = clockService_.commonCount();
	settingsPage_.setContext(c);
}

void ExperienceRuntime::openSettingsPage() {
	if (settingsPage_.isOpen() || immersive_) return;
	if (installation_.energyAdjusting()) { // never leave ENERGY capturing steps behind the page
		installation::Request r;
		r.kind = installation::Request::Kind::EndEnergyAdjust;
		installation_.submit(r);
	}
	saveStatus_.clear();
	saveFailed_ = false;
	clockResult_.clear();
	clockResultFailed_ = false;
	lastOkInputAt_ = steadySeconds(); // swallow the OK (and its key-repeat) that opened the page
	hudCompositorBridge_.orbitEditorBack(); // OC-005: an open Orbit editor yields to the Settings page (restores)
	hudCompositorBridge_.exitInteractive(); // an active interactive region yields to the Settings page
	refreshSettingsContext(true);
	settingsPage_.open(settingsPage_.context());
	ofSetEscapeQuitsApp(false); // ESC returns to the experience while the page is open
	escapeQuitSuppressed_ = true;
	ofLogNotice("Settings") << "page opened (scene/media auto-cycle paused; artwork keeps running)";
}

void ExperienceRuntime::closeSettingsPage() {
	if (!settingsPage_.isOpen()) return;
	settingsPage_.close();
	settingsClosedAt_ = steadySeconds();
	installation_.setAutomationSuspended(false); // fresh period; no catch-up burst
	ofLogNotice("Settings") << "page closed (automation resumes with a fresh period)";
}

void ExperienceRuntime::performSettingsAction(const settingspage::Action& a) {
	using K = settingspage::Action::Kind;
	installation::Request r;
	const installation::Settings& s = installation_.settings();
	switch (a.kind) {
		case K::Close: closeSettingsPage(); return;
		case K::SetSceneAuto:
			r.kind = installation::Request::Kind::SetSceneAutoCycle; r.flag = a.flag; break;
		case K::SetSceneInterval:
			r.kind = installation::Request::Kind::SetSceneAutoCycle; r.flag = s.sceneAutoCycle.enabled; r.durationSeconds = a.seconds; break;
		case K::SetMediaAuto:
			r.kind = installation::Request::Kind::SetMediaAutoCycle; r.flag = a.flag; break;
		case K::SetMediaInterval:
			r.kind = installation::Request::Kind::SetMediaAutoCycle; r.flag = s.mediaAutoCycle.enabled; r.durationSeconds = a.seconds; break;
		case K::SetClock24:
			r.kind = installation::Request::Kind::SetClock24Hour; r.flag = a.flag; break;
		case K::SetSystemMode:
			r.kind = installation::Request::Kind::SetSystemMode; r.text = a.text; break;
		case K::SetMinimalBackground:
			r.kind = installation::Request::Kind::SetMinimalBackground; r.flag = a.dark; r.steps = a.index; break;
		case K::SetButtonLabels:
			r.kind = installation::Request::Kind::SetButtonLabels; r.text = a.text; break;
		case K::ApplyClock: {
			const systemclock::ApplyResult res = clockService_.apply(a.change);
			clockResult_ = res.message;
			clockResultFailed_ = !res.ok;
			ofLogNotice("Settings") << "clock apply: " << (res.ok ? "ok" : "FAILED") << " - " << res.message;
			refreshSettingsContext(true); // show the OS's actual state, never the requested one
			return;
		}
	}
	const installation::Result result = installation_.submit(r);
	if (!result.accepted) {
		saveStatus_ = "REJECTED (" + result.code + ")";
		saveFailed_ = true;
		return;
	}
	// Confirmed edit: persist now (not on every D-pad step) and report the outcome.
	if (a.kind == K::SetButtonLabels) applyButtonLabelMode();
	if (a.kind == K::SetSystemMode) applySystemMode(a.text, "Settings page");
	else if (a.kind == K::SetMinimalBackground && minimaltheme::isMinimalId(installation_.settings().systemMode)) {
		applySystemMode(installation_.settings().systemMode, "Settings page (background)");
	}
	if (installation_.takeSettingsChanged()) saveInstallationSettings();
}

bool ExperienceRuntime::handleSettingsKey(int key) {
	if (!settingsPage_.isOpen()) {
		if (inputRouter_.isSettingsToggle(key) && !immersive_) { openSettingsPage(); return true; }
		return false;
	}
	// Open: the page is the sole input consumer — nothing reaches scene, HUD
	// focus or runtime commands (ESC returns to the experience, never exits).
	if (inputRouter_.isSettingsToggle(key)) { closeSettingsPage(); return true; }
	if (auto d = inputRouter_.translateToDpad(key)) {
		if (*d == DpadInput::Ok) { // OK is debounced; Left/Right keep auto-repeat for stepping
			const double now = steadySeconds();
			if (lastOkInputAt_ >= 0.0 && now - lastOkInputAt_ < 0.25) return true;
			lastOkInputAt_ = now;
		}
		for (const auto& action : settingsPage_.handle(*d)) performSettingsAction(action);
		if (settingsPage_.isOpen()) refreshSettingsContext(false);
	}
	return true;
}

void ExperienceRuntime::loadInstallationSettings() {
	installation::Settings s;
	if (!installationSettingsPath_.empty() && ofFile::doesFileExist(installationSettingsPath_, false)) {
		ofJson j = ofLoadJson(installationSettingsPath_);
		auto num = [](const ofJson& o, const char* k, float d) { return o.contains(k) && o[k].is_number() ? o[k].get<float>() : d; };
		auto flag = [](const ofJson& o, const char* k, bool d) { return o.contains(k) && o[k].is_boolean() ? o[k].get<bool>() : d; };
		if (j.contains("sceneAutoCycle") && j["sceneAutoCycle"].is_object()) {
			s.sceneAutoCycle.enabled = flag(j["sceneAutoCycle"], "enabled", s.sceneAutoCycle.enabled);
			s.sceneAutoCycle.durationSeconds = num(j["sceneAutoCycle"], "durationSeconds", s.sceneAutoCycle.durationSeconds);
		}
		if (j.contains("mediaAutoCycle") && j["mediaAutoCycle"].is_object()) {
			s.mediaAutoCycle.enabled = flag(j["mediaAutoCycle"], "enabled", s.mediaAutoCycle.enabled);
			s.mediaAutoCycle.durationSeconds = num(j["mediaAutoCycle"], "durationSeconds", s.mediaAutoCycle.durationSeconds);
		}
		s.launchFullscreen = flag(j, "launchFullscreen", s.launchFullscreen);
		s.clock24Hour = flag(j, "clock24Hour", s.clock24Hour);
		if (j.contains("buttonLabels") && j["buttonLabels"].is_string()) s.buttonLabels = j["buttonLabels"].get<std::string>();
		if (j.contains("systemMode") && j["systemMode"].is_string()) s.systemMode = j["systemMode"].get<std::string>();
		if (j.contains("minimalBackground") && j["minimalBackground"].is_object()) {
			auto idx = [](const ofJson& o, const char* k, int d) { return o.contains(k) && o[k].is_number_integer() ? o[k].get<int>() : d; };
			s.minimalBackgroundLight = idx(j["minimalBackground"], "light", s.minimalBackgroundLight);
			s.minimalBackgroundDark = idx(j["minimalBackground"], "dark", s.minimalBackgroundDark);
		}
		if (j.contains("energy") && j["energy"].is_object()) s.energyTarget = num(j["energy"], "target", s.energyTarget);
		// OC-005 Stage 2: committed Orbit observations. A missing/mistyped value keeps the documented default
		// (A = fx/avg, B = raw/avg) for that field; an unknown token is rejected by installation_.load() below.
		if (j.contains("orbitObservations") && j["orbitObservations"].is_object()) {
			auto sel = [](const ofJson& o, const char* k, installation::Settings::OrbitSelection& out) {
				if (!o.contains(k) || !o[k].is_object()) return;
				const ofJson& e = o[k];
				if (e.contains("source") && e["source"].is_string()) out.source = e["source"].get<std::string>();
				if (e.contains("metric") && e["metric"].is_string()) out.metric = e["metric"].get<std::string>();
			};
			sel(j["orbitObservations"], "a", s.orbitA);
			sel(j["orbitObservations"], "b", s.orbitB);
		}
		ofLogNotice("Installation") << "settings loaded from " << installationSettingsPath_;
	} else {
		ofLogNotice("Installation") << "settings: defaults" << (installationSettingsPath_.empty() ? " (persistence off)" : " (no file yet)");
	}
	for (const auto& field : installation_.load(s)) ofLogWarning("Installation") << "invalid " << field << " in settings; default kept";
	const installation::Settings& applied = installation_.settings();
	ofLogNotice("Installation") << "applied: sceneAutoCycle " << (applied.sceneAutoCycle.enabled ? "on" : "off") << " "
		<< applied.sceneAutoCycle.durationSeconds << " s, mediaAutoCycle " << (applied.mediaAutoCycle.enabled ? "on" : "off") << " "
		<< applied.mediaAutoCycle.durationSeconds << " s, launchFullscreen " << (applied.launchFullscreen ? "on" : "off")
		<< ", energy.target " << applied.energyTarget;
	applyOrbitSettingsToHud();
	installationLoaded_ = true;
}

// OC-005 Stage 2: the committed Orbit selection (installation settings) -> the HUD-owned configuration.
void ExperienceRuntime::applyOrbitSettingsToHud() {
	const installation::Settings& s = installation_.settings();
	hudpresent::OrbitConsoleConfig c;
	hudpresent::orbitFromTokens(s.orbitA.source.c_str(), s.orbitA.metric.c_str(), c.a); // validated at load; defaults on any miss
	hudpresent::orbitFromTokens(s.orbitB.source.c_str(), s.orbitB.metric.c_str(), c.b);
	hudCompositorBridge_.orbitUi().setCommitted(c);
	ofLogNotice("Installation") << "orbit observations: A " << s.orbitA.source << "/" << s.orbitA.metric << ", B " << s.orbitB.source << "/" << s.orbitB.metric;
}

// Done on the Orbit editor: commit ONE observation to the installation settings (the other is untouched; an
// unchanged value writes nothing). Preview and Escape never reach here.
void ExperienceRuntime::commitOrbitObservation(hudpresent::OrbitObs which) {
	const hudpresent::OrbitObservationConfig& c = hudCompositorBridge_.orbitUi().config().at(which);
	installation::Request r;
	r.kind = installation::Request::Kind::SetOrbitObservation;
	r.flag = which == hudpresent::OrbitObs::B;
	r.text = std::string(hudpresent::orbitSourceToken(c.source)) + "." + hudpresent::orbitMetricToken(c.metric);
	installation_.submit(r);
	if (installation_.takeSettingsChanged()) saveInstallationSettings();
}

void ExperienceRuntime::saveInstallationSettings() {
	if (installationSettingsPath_.empty()) { saveStatus_ = "not saved (persistence off)"; saveFailed_ = false; return; }
	const installation::Settings& s = installation_.settings();
	ofJson j;
	j["sceneAutoCycle"] = {{"enabled", s.sceneAutoCycle.enabled}, {"durationSeconds", s.sceneAutoCycle.durationSeconds}};
	j["mediaAutoCycle"] = {{"enabled", s.mediaAutoCycle.enabled}, {"durationSeconds", s.mediaAutoCycle.durationSeconds}};
	j["launchFullscreen"] = s.launchFullscreen;
	j["energy"] = {{"target", s.energyTarget}};
	j["clock24Hour"] = s.clock24Hour;
	j["buttonLabels"] = s.buttonLabels;
	j["systemMode"] = s.systemMode;
	j["minimalBackground"] = {{"light", s.minimalBackgroundLight}, {"dark", s.minimalBackgroundDark}};
	j["orbitObservations"] = {{"a", {{"source", s.orbitA.source}, {"metric", s.orbitA.metric}}},
		{"b", {{"source", s.orbitB.source}, {"metric", s.orbitB.metric}}}};
	// Write a sibling temp file, then rename over the target: a crash or full
	// disk can never leave a truncated settings file behind.
	namespace fs = std::filesystem;
	const std::string tmp = installationSettingsPath_ + ".tmp";
	std::error_code ec;
	bool ok = false;
	{
		std::ofstream out(tmp, std::ios::trunc);
		if (out) { out << j.dump(4) << "\n"; out.flush(); ok = static_cast<bool>(out); }
	}
	if (ok) { fs::rename(tmp, installationSettingsPath_, ec); ok = !ec; }
	if (!ok) fs::remove(tmp, ec);
	saveFailed_ = !ok;
	saveStatus_ = ok ? "saved" : "SAVE FAILED (cannot write " + installationSettingsPath_ + ")";
	if (ok) ofLogNotice("Installation") << "settings saved to " << installationSettingsPath_;
	else ofLogError("Installation") << saveStatus_;
}

// Start of every frame — the same point key events reach the runtime, so a
// scheduled NextScene takes exactly the path a ']' press takes.
void ExperienceRuntime::tickInstallation(float dt) {
	installation_.setAutomationSuspended(settingsPage_.isOpen()); // Settings page open: scene auto-cycle paused
	for (const auto& result : installation_.drainQueue()) {
		if (!result.accepted) ofLogWarning("Installation") << "queued request rejected: " << result.code;
	}
	if (installation_.takeFullscreenToggle()) {
		ofToggleFullscreen();
		ofLogNotice("Installation") << "fullscreen toggle requested";
	}
	// Any completed activation (manual or automatic) restarts the period.
	if (sceneManager_.activationSucceededThisFrame()) installation_.noteSceneChanged();
	const bool sceneIdle = sceneManager_.hasProductionScenes() && sceneManager_.status().transitionPhase == SceneTransitionPhase::Idle;
	if (installation_.tickSceneCycle(dt, sceneIdle)) {
		ofLogNotice("Installation") << "scene auto-cycle -> NextScene";
		handleRuntimeCommand(RuntimeCommand::NextScene);
	}
	// ENERGY: smoothed once per frame; Quadrant is the one mapped scene in
	// v0.1 (re-applied every frame, so it survives activation resets).
	quadrantScene_.setEnergy(installation_.tickEnergy(dt));
	if (installation_.takeSettingsChanged()) saveInstallationSettings();
}

InstallationStatus ExperienceRuntime::buildInstallationStatus() const {
	const installation::Settings& s = installation_.settings();
	InstallationStatus st;
	st.sceneAutoCycleEnabled = s.sceneAutoCycle.enabled;
	st.sceneAutoCycleSeconds = s.sceneAutoCycle.durationSeconds;
	st.sceneAutoCycleElapsedSeconds = installation_.sceneCycleElapsed();
	st.mediaAutoCycleEnabled = s.mediaAutoCycle.enabled;
	st.mediaAutoAdvanceActive = runtimeServices_.video().automaticAdvanceEnabled();
	st.mediaAutoCycleSeconds = s.mediaAutoCycle.durationSeconds;
	st.fullscreen = ofGetWindowMode() == OF_FULLSCREEN;
	st.launchFullscreen = s.launchFullscreen;
	st.energyTarget = s.energyTarget;
	st.energy = installation_.energySmoothed();
	st.energyAdjusting = installation_.energyAdjusting();
	return st;
}

void ExperienceRuntime::establishGlobalRenderingBaseline() {
	// Moved out of scene setup() code per the placement/readiness probe's
	// discovery report §6 — every one of these was previously set
	// independently, inconsistently, or (for ofDisableArbTex) redundantly
	// by individual scenes.
	ofSetVerticalSync(true);
	ofSetFrameRate(kProvisionalDevelopmentFrameRateFps);

	// blob-region-prototype's own setup() comment explains why this is
	// required globally: every shared ShaderLibrary/video-effects shader
	// assumes normalized [0,1] sampler2D texcoords; without this call, OF
	// defaults to GL_TEXTURE_RECTANGLE textures and shaded output samples
	// as solid black.
	ofDisableArbTex();
}

void ExperienceRuntime::setup() {
	// HUD activity log: wrap the current logger channel (the console prints
	// exactly as before) and keep the last lines for HudFrameData::activityLog.
	if (!logCapture_) {
		logCapture_ = std::make_shared<HudLogCapture>(ofGetLoggerChannel());
		ofSetLoggerChannel(logCapture_);
	}

	establishGlobalRenderingBaseline();

	// VideoPlaybackService setup, per Implement-Shared-Video-Playback-
	// System-Agent-Prompt.md §2/§9's approved ownership (RuntimeServices
	// owns VideoPlaybackService) — moved ahead of SceneManager setup/
	// activation (Blob First Complete Production Migration): blobScene_
	// consumes runtimeServices_.video() directly (the ONE canonical
	// instance — see BlobProductionScene's constructor comment for why
	// this must never be a second/local instance), so the video service
	// must be configured before the scene that reads it is set up and
	// activated below.
	VideoPlaybackService::Config videoConfig;
	videoConfig.mediaRoot = ofToDataPath("../../../../assets/shared/media", true);
	// Dwell policy (VideoSelectionPolicy.h): 30 s minimum dwell; long clips
	// (> 2 x 120 s) play 120 s resume windows that advance across visits.
	videoConfig.holdDurationSeconds = 30.0f;
	videoConfig.maxDwellSeconds = 120.0f;
	videoConfig.automaticAdvance = true;
	// Verification only: EXPERIENCE_RUNTIME_VIDEO_DWELL="<min>,<max>" scales
	// the policy down so the real catalog (longest clip ~100 s) contains
	// "long" clips. Never set in a normal run.
	if (const char* dwell = std::getenv("EXPERIENCE_RUNTIME_VIDEO_DWELL")) {
		float minDwell = 0.0f;
		float maxDwell = 0.0f;
		if (std::sscanf(dwell, "%f,%f", &minDwell, &maxDwell) == 2 && minDwell > 0.0f && maxDwell >= minDwell) {
			videoConfig.holdDurationSeconds = minDwell;
			videoConfig.maxDwellSeconds = maxDwell;
			ofLogWarning("ExperienceRuntime") << "verification dwell override: min " << minDwell << " s, max " << maxDwell << " s";
		}
	}
	// Verification only (OC-003 attribution): EXPERIENCE_RUNTIME_VIDEO_STARTUP_MEDIA_ID=<canonical mediaId> selects the
	// startup item through the normal selection policy; an unknown/unplayable id is rejected visibly (no fallback).
	// Unset = unchanged random startup. Never set in a normal run.
	if (const char* id = std::getenv("EXPERIENCE_RUNTIME_VIDEO_STARTUP_MEDIA_ID")) {
		if (*id) {
			videoConfig.verificationStartupMediaId = id;
			ofLogWarning("ExperienceRuntime") << "verification startup media override: " << id;
		}
	}
	runtimeServices_.setup(videoConfig);

	// Whether the production scene pair (vs. the tooling-only FakeScene) is
	// what gets set up/activated below is decided by the CALLER, before this
	// method runs — see installBlobProductionScene()'s own comment for why
	// (GlRestorationHarness must keep exercising FakeScene unmodified).

	// SceneServices construction.
	//
	// Engineering Session 2 §3.1 — exact meaning, documented here per that
	// task's instruction, near the public contract and this runtime setup
	// path:
	//
	//   SceneServices::canvasSize        = canonical runtime composition canvas
	//   IEcopunkScene::nativeRenderSize() = active scene native render size
	//   ExperienceRuntime scene FBO size  = active scene native render size
	//
	// canvasSize is NOT a synonym for the scene FBO size — the FBO always
	// follows nativeRenderSize() (see below, "glm::ivec2 nativeSize =
	// sceneManager_.activeSceneNativeRenderSize()"), deliberately
	// independent of whatever canvasSize reports. For the current product
	// the canonical composition canvas is 1280x720 (FakeScene reports a
	// deliberately different 320x180 nativeRenderSize() to prove this
	// independence holds in code, not just in comments).
	SceneServices services;
	services.canvasSize = glm::ivec2(1280, 720);

	// Canonical-shaped paths per Scene-HUD-Contract-v1.md §12's asset
	// tree. sharedMediaRoot resolves to the REAL canonical media root
	// (assets/shared/media/, Shared Video Playback Engineering Session 2,
	// Task C); videoConfig.mediaRoot above is populated independently, since
	// VideoPlaybackService takes an already-OF-resolved path via its own
	// Config. RT-003: sceneAssetRoot is filled in per scene by SceneManager
	// ("assets/scenes/<sceneId>/", derived from each registered scene's own
	// stable ID when that scene is set up) — still an inert, unread value
	// for every current scene.
	services.sceneAssetRoot = "";
	services.sharedMediaRoot = ofToDataPath("../../../../assets/shared/media", true);
	services.sharedEffectAssetRoot = "assets/shared/video-effects/";

	// PerfInstrumentation does not exist anywhere in this repo yet — left
	// null rather than invented here.
	services.perf = nullptr;

	sceneManager_.setup(services);        // sets up the startup scene only
	sceneManager_.activateScene();        // caches SceneCapabilities exactly once here

	// Temporal's immutable ambient textures are prepared here, once, before
	// the first frame — not on the first switch into Temporal, where loading
	// them froze the runtime for ~1.6 s. Scene setup/activation stay lazy and
	// unchanged (SceneManager still sets Temporal up on first use).
	if (productionScenesInstalled_) {
		RT004_SCOPE("Temporal.prepareImmutableAssets");
		temporalScene_.prepareImmutableAssets();
	}

	glm::ivec2 nativeSize = sceneManager_.activeSceneNativeRenderSize();
	sceneFbo_.allocate(nativeSize.x, nativeSize.y, GL_RGBA);
	presentationCounters_.sceneFboAllocations++;

	// Orbit Console Scene Sampling v0.1 switches (read once).
	if (const char* v = std::getenv("EXPERIENCE_RUNTIME_SCENE_COLOR")) sceneColorEnabled_ = std::string(v) != "0";
	if (const char* v = std::getenv("EXPERIENCE_RUNTIME_SCENE_COLOR_MODE"))
		sceneColor_.setMode(std::string(v) == "async" ? scenecolorsampling::ReadbackMode::Async
			: std::string(v) == "deferred" ? scenecolorsampling::ReadbackMode::Deferred : scenecolorsampling::ReadbackMode::Sync);
	if (const char* v = std::getenv("EXPERIENCE_RUNTIME_SCENE_COLOR_STATS")) sceneColorLogStats_ = std::string(v) == "1";
	// OC-003 dominant/vibrant colour (Tier C additive): OFF unless exactly "1". Applies to the FX sampler and, when RAW
	// sampling is itself enabled, to the RAW sampler. Needs the GPU linear path; otherwise DOM/VIB stay NOT AVAILABLE.
	const bool predominantRequested = rawvideo::videoColorEnabledFromEnv(std::getenv("EXPERIENCE_RUNTIME_PREDOMINANT_COLOR"));
	sceneColor_.setPredominantEnabled(predominantRequested);
	// OC-003 EXPERIMENTAL readback format for BOTH samplers: EXPERIENCE_RUNTIME_SCENE_COLOR_READBACK=float (default)|half|half-verify.
	// Anything else is reported and the baseline float path is used (stated in the log, never silent).
	scenecolorsampling::ReadbackFormat readbackFormat = scenecolorsampling::ReadbackFormat::Float;
	if (const char* v = std::getenv("EXPERIENCE_RUNTIME_SCENE_COLOR_READBACK")) {
		const std::string s = v;
		if (s == "half") readbackFormat = scenecolorsampling::ReadbackFormat::Half;
		else if (s == "half-verify") readbackFormat = scenecolorsampling::ReadbackFormat::HalfVerify;
		else if (s != "float") ofLogError("SceneColor") << "EXPERIENCE_RUNTIME_SCENE_COLOR_READBACK='" << s << "' is not float|half|half-verify: using the float baseline";
	}
	sceneColor_.setReadbackFormat(readbackFormat);
	videoColor_.setReadbackFormat(readbackFormat);
	if (readbackFormat != scenecolorsampling::ReadbackFormat::Float) ofLogWarning("SceneColor") << "EXPERIMENTAL readback format: " << sceneColor_.readbackName();
	if (predominantRequested) ofLogNotice("SceneColor") << "DOMINANT/VIBRANT colour sampling REQUESTED (EXPERIENCE_RUNTIME_PREDOMINANT_COLOR=1)";
	// OC-003B RAW VIDEO sampler: OFF unless exactly "1". Deferred readback by default (no GPU stall).
	videoColorEnabled_ = rawvideo::videoColorEnabledFromEnv(std::getenv("EXPERIENCE_RUNTIME_VIDEO_COLOR"));
	if (videoColorEnabled_) {
		videoColor_.setSourceKind(rawvideo::SourceKind::DecodedVideo);
		videoColor_.setPredominantEnabled(predominantRequested);
		const char* m = std::getenv("EXPERIENCE_RUNTIME_VIDEO_COLOR_MODE");
		videoColor_.setMode(m && std::string(m) == "sync" ? scenecolorsampling::ReadbackMode::Sync
			: m && std::string(m) == "async" ? scenecolorsampling::ReadbackMode::Async : scenecolorsampling::ReadbackMode::Deferred);
		if (const char* v = std::getenv("EXPERIENCE_RUNTIME_VIDEO_COLOR_STATS")) videoColorStats_ = std::string(v) == "1";
		if (const char* v = std::getenv("EXPERIENCE_RUNTIME_VIDEO_COLOR_DIAG")) videoColorDiag_ = std::string(v) == "1";
		ofLogNotice("VideoColor") << "RAW VIDEO sampling ENABLED (EXPERIENCE_RUNTIME_VIDEO_COLOR=1), mode="
			<< (videoColor_.mode() == scenecolorsampling::ReadbackMode::Sync ? "sync" : videoColor_.mode() == scenecolorsampling::ReadbackMode::Async ? "async" : "deferred");
	}

	// OC-003-F acquisition-isolation diagnostics. EVERY control below is off unless set; with none of them set nothing here changes behaviour.
	//   EXPERIENCE_RUNTIME_CPU_RAW=1            CPU RAW prototype: reduces the decoder's existing CPU-resident frame (no GPU work, no second decoder)
	//   EXPERIENCE_RUNTIME_CPU_RAW_STATS=1      10 s `stream=CPURAW` stat line
	//   EXPERIENCE_RUNTIME_PALETTE_DIAG=<dir>   palette analyzer sink on the grids the active samplers already acquired
	//   EXPERIENCE_RUNTIME_PALETTE_DIAG_GRIDS=N / _EVERY=seconds   bounded grid snapshots (defaults 48 per stream / 10 s)
	cpuRawEnabled_ = rawvideo::videoColorEnabledFromEnv(std::getenv("EXPERIENCE_RUNTIME_CPU_RAW"));
	if (cpuRawEnabled_) {
		cpuRaw_.setPredominant(predominantRequested);
		cpuRawStats_ = rawvideo::videoColorEnabledFromEnv(std::getenv("EXPERIENCE_RUNTIME_CPU_RAW_STATS"));
	}
	if (const char* dir = std::getenv("EXPERIENCE_RUNTIME_PALETTE_DIAG")) {
		if (*dir) {
			int cap = 48; double every = 10.0;
			if (const char* g = std::getenv("EXPERIENCE_RUNTIME_PALETTE_DIAG_GRIDS")) cap = std::max(0, std::atoi(g));
			if (const char* e = std::getenv("EXPERIENCE_RUNTIME_PALETTE_DIAG_EVERY")) every = std::max(0.5, std::atof(e));
			if (paletteSink_.open(dir, cap, every)) {
				paletteOn_ = true;
				sceneColor_.setPaletteSink(&paletteSink_, palette::kFx);
				videoColor_.setPaletteSink(&paletteSink_, palette::kRawGpu);
			} else {
				ofLogError("AcquisitionMode") << "EXPERIENCE_RUNTIME_PALETTE_DIAG='" << dir << "' could not be created: palette analysis NOT active";
			}
		}
	}
	if (cpuRawEnabled_ || paletteOn_ || std::getenv("EXPERIENCE_RUNTIME_FRAME_PHASE_PROBE")) {
		// One machine-readable line per process: which acquisition paths this run really has (checked by scripts/oc003/verify_run.py).
		ofLogNotice("AcquisitionMode") << "fxGpu=" << (sceneColorEnabled_ ? "on" : "off") << " rawGpu=" << (videoColorEnabled_ ? "on" : "off")
			<< " cpuRaw=" << (cpuRawEnabled_ ? "on" : "off") << " palette=" << (paletteOn_ ? "on" : "off")
			<< " predominant=" << (predominantRequested ? "on" : "off") << " contract=oc003f-1";
	}

	// Engineering Session 2: real HudCompositor bridge setup — establishes
	// the canonical 1280x720 canvas (see HudCompositorBridge's own header
	// comment for why the window is already sized to match this exactly).
	hudCompositorBridge_.setup();

	// HUD-004 first shell-only skin loader. HUD asset paths reach the
	// compositor through HudAssetPaths, never SceneServices (Scene/HUD
	// Contract v1 §12), resolved the same host-relative way as
	// sharedMediaRoot above. The expected Blueprint pair is supplied
	// explicitly by this caller — the frozen HUD-006 identity (DEC-019) —
	// because the validator deliberately has no default.
	{
		HudAssetPaths hudPaths;
		hudPaths.skinsRoot = ofToDataPath("../../../../assets/hud/skins", true);
		hudskin::SkinManifestValidationPolicy expected;
		expected.expectedBlueprintId = "ecopunk-hud-blueprint";
		expected.expectedBlueprintVersion = 2u; // HUD-006 v2: v1 skins are rejected (SKIN_BLUEPRINT_VERSION_MISMATCH)
		hudCompositorBridge_.configureSkins(hudPaths, expected);

		// Shell palettes must be loaded before the first applySkin() below.
		loadShellPalettes(ofToDataPath("../../../../assets/hud/themes/shell-palettes.json", true));

		// Skin selection. EXPERIENCE_RUNTIME_HUD_SKIN (explicit) wins and fixes
		// the skin; otherwise, on normal runs, the day/night schedule picks it
		// by local time. Neither -> functional wireframe, as before. The 'v'
		// key cycles skins live (atomic loader replacement), 'V' resumes the
		// schedule — see ExperienceRuntime::keyPressed.
		if (skinScheduleEnabled_) loadSkinSchedule(ofToDataPath("../../../../assets/hud/skins/skin-schedule.json", true));
		const char* skin = std::getenv("EXPERIENCE_RUNTIME_HUD_SKIN");
		if (skin != nullptr && skin[0] != '\0') {
			applySkin(skin, "EXPERIENCE_RUNTIME_HUD_SKIN");
		} else if (skinScheduleEnabled_ && skinSchedule_.valid()) {
			skinAuto_ = true;
			tickSkinSchedule(true);
		} else {
			ofLogNotice("HudSkin") << "no HUD skin selected (EXPERIENCE_RUNTIME_HUD_SKIN unset, no schedule): functional wireframe";
		}
	}

	// Typography: the preset saved by the dev Live Typography Playground applies to the
	// production runtime too (fonts, sizes, per-skin colours). The playground's editing
	// keys and overlay are on only when explicitly requested; EXPERIENCE_RUNTIME_TYPOGRAPHY=off
	// disables the preset (stock HUD fonts).
	{
		const char* dirEnv = std::getenv("EXPERIENCE_RUNTIME_TYPOGRAPHY_DIR");
		const char* offEnv = std::getenv("EXPERIENCE_RUNTIME_TYPOGRAPHY");
		const bool off = !typographyPresetEnabled_ || (offEnv != nullptr && std::string(offEnv) == "off");
		const bool editing = TypographyPlayground::isRequested();
		const std::string dir = dirEnv ? std::string(dirEnv) : ofToDataPath("../../../../assets/hud/dev-typography", true);
		if (editing || (!off && std::filesystem::exists(std::filesystem::path(dir) / "preset.json"))) {
			typography_ = std::make_unique<TypographyPlayground>();
			typography_->load(dir);
			if (!editing) typography_->setApplyOnly();
			hudCompositorBridge_.setLabelTypography(typography_.get());
			typography_->setSkin(currentSkin_, currentSkin_.empty() ? std::string("night") : skinSchedule_.paletteFor(currentSkin_));
			ofLogNotice("Typography") << (editing ? "DEV typography playground ENABLED (EXPERIENCE_RUNTIME_TYPOGRAPHY_PLAYGROUND=1)"
			                                      : "typography preset applied from " + dir);
		}
	}

	// Installation Controls & ENERGY v0.1: settings (validated) and the
	// persistent launch preference. applyMediaCadence() applies the media
	// cadence every frame from here on.
	loadInstallationSettings();
	// System Mode: minimal-theme colours (optional file), then the saved mode. An explicit
	// EXPERIENCE_RUNTIME_HUD_SKIN still wins and fixes the skin for that launch.
	applyButtonLabelMode();
	loadMinimalThemes(ofToDataPath("../../../../assets/hud/themes/minimal-theme.json", true));
	{
		const char* envSkin = std::getenv("EXPERIENCE_RUNTIME_HUD_SKIN");
		const std::string& mode = installation_.settings().systemMode;
		if ((envSkin == nullptr || envSkin[0] == '\0') && mode != "auto") applySystemMode(mode, "saved setting");
	}
	// RT-007 precedence for NATIVE host fullscreen at startup (one authority
	// per launch): an explicit EXPERIENCE_RUNTIME_FULLSCREEN=1/0 wins — main.cpp
	// already created the window in that mode — otherwise the persisted
	// Installation Controls launchFullscreen preference applies. The
	// preference itself is never rewritten by the override.
	if (const auto overrideFullscreen = fullscreenOverrideFromEnv()) {
		ofLogNotice("Presentation") << "EXPERIENCE_RUNTIME_FULLSCREEN=" << (*overrideFullscreen ? "1" : "0")
			<< " overrides launchFullscreen (" << (installation_.settings().launchFullscreen ? "on" : "off") << ")";
	} else if (installation_.settings().launchFullscreen) {
		ofSetFullscreen(true);
		ofLogNotice("Installation") << "launchFullscreen: starting fullscreen";
	}

	didSetup_ = true;
}

bool ExperienceRuntime::loadSkinSchedule(const std::string& path) {
	ofJson j;
	try {
		if (ofFile::doesFileExist(path, false)) j = ofLoadJson(path);
	} catch (...) {
		j = ofJson();
	}
	if (!j.is_object()) {
		ofLogNotice("HudSkin") << "no skin schedule at " << path;
		return false;
	}
	HudSkinSchedule s;
	if (auto m = HudSkinSchedule::parseClock(j.value("dayStart", std::string("07:00")))) s.dayStartMinute = *m;
	if (auto m = HudSkinSchedule::parseClock(j.value("nightStart", std::string("19:00")))) s.nightStartMinute = *m;
	auto readGroup = [&](const char* key, HudSkinSchedule::Group& g, const char* defaultPalette) {
		g.palette = defaultPalette;
		if (!j.contains(key) || !j[key].is_object()) return;
		g.palette = j[key].value("palette", std::string(defaultPalette));
		if (j[key].contains("skins") && j[key]["skins"].is_array()) {
			for (const auto& id : j[key]["skins"]) if (id.is_string()) g.skins.push_back(id.get<std::string>());
		}
	};
	readGroup("day", s.day, "day");
	readGroup("night", s.night, "night");
	skinSchedule_ = s;
	ofLogNotice("HudSkin") << "skin schedule: day " << s.day.skins.size() << " skin(s) from " << s.dayStartMinute / 60 << ":"
		<< ofToString(s.dayStartMinute % 60, 2, '0') << ", night " << s.night.skins.size() << " skin(s) from "
		<< s.nightStartMinute / 60 << ":" << ofToString(s.nightStartMinute % 60, 2, '0');
	return s.valid();
}

bool ExperienceRuntime::applySkin(const std::string& skinId, const std::string& reason) {
	if (skinId.empty()) return false;
	const auto outcome = hudCompositorBridge_.requestSkin(skinId); // atomic: a failure keeps the current skin
	if (outcome.activated) {
		leaveMinimal(); // a skin replaces the minimal look (viewport style, typography preset)
		currentSkin_ = skinId;
		const std::string palette = skinSchedule_.paletteFor(skinId);
		const auto shell = shellPalettes_.find(skinId);
		if (shell != shellPalettes_.end()) {
			const shelltheme::Tokens t = shelltheme::derive(shell->second);
			auto col = [](const shelltheme::Rgba& c) { return ofColor(c.r, c.g, c.b, c.a); };
			hudpresent::HudWidgetColors colors = hudpresent::hudPaletteNamed(palette); // keeps daytime/button settings
			colors.primary = col(t.primary);
			colors.secondary = col(t.secondary);
			colors.accent = col(t.accent);
			colors.muted = col(t.muted);
			colors.caption = col(t.caption);
			colors.background = col(t.panel);
			colors.warning = col(t.warning);
			colors.dataA = col(t.dataA);
			colors.dataB = col(t.dataB);
			colors.shadow = col(t.shadow);
			if (shell->second.mode == shelltheme::Mode::Dark) colors.buttonText = col(t.primary); // light labels on the dark baked faces; day keeps its own
			hudCompositorBridge_.setColors(colors);
			// Under the video inside the media screen; skins draw their own frame, so the border stays off.
			hudCompositorBridge_.setViewportStyle(ofColor(t.viewportBacking.r, t.viewportBacking.g, t.viewportBacking.b, 255), col(t.border), false);
		} else {
			hudCompositorBridge_.setColors(hudpresent::hudPaletteNamed(palette));
		}
		if (typography_) typography_->setSkin(skinId, palette);
		ofLogNotice("HudSkin") << "skin '" << skinId << "' (" << palette << " palette) — " << reason;
	}
	return outcome.activated;
}

void ExperienceRuntime::applyButtonLabelMode() {
	const std::string& v = installation_.settings().buttonLabels;
	hudCompositorBridge_.setButtonLabelMode(v == "icon" ? 1 : v == "both" ? 2 : 0);
}

void ExperienceRuntime::leaveMinimal() {
	if (!minimalActive_) return;
	minimalActive_ = false;
	canvasBackground_ = ofColor(12, 12, 12);
	hudCompositorBridge_.setViewportStyle(ofColor(20, 26, 24, 255), ofColor(255), false);
	if (typography_) hudCompositorBridge_.setLabelTypography(typography_.get());
}

bool ExperienceRuntime::applySystemMode(const std::string& modeId, const std::string& reason) {
	if (modeId == "auto") {
		leaveMinimal();
		if (skinSchedule_.valid()) {
			skinAuto_ = true;
			tickSkinSchedule(true);
		}
		ofLogNotice("SystemMode") << "auto (day/night skins) — " << reason;
		return true;
	}
	if (const auto minimal = minimaltheme::parseMinimalId(modeId)) {
		const installation::Settings& s = installation_.settings();
		const int bg = minimal->group == minimaltheme::Group::Dark ? s.minimalBackgroundDark : s.minimalBackgroundLight;
		const auto d = minimaltheme::resolve(themes_, modeId, bg);
		if (!d) {
			ofLogWarning("SystemMode") << "unknown minimal theme '" << modeId << "'; keeping the current look";
			return false;
		}
		auto col = [](const minimaltheme::Rgba& c) { return ofColor(c.r, c.g, c.b, c.a); };
		skinAuto_ = false;
		hudCompositorBridge_.clearSkin();
		currentSkin_.clear();
		hudpresent::HudWidgetColors colors;
		colors.primary = col(d->primary);
		colors.secondary = col(d->secondary);
		colors.accent = col(d->accent);
		colors.muted = col(d->muted);
		colors.caption = col(d->caption);
		colors.background = col(d->panel);
		colors.warning = col(d->warning);
		colors.shadow = col(d->shadow);
		colors.buttonText = col(d->buttonText);
		colors.buttonShadow = col(d->buttonShadow);
		hudCompositorBridge_.setColors(colors);
		hudCompositorBridge_.setViewportStyle(ofColor(d->viewportBacking.r, d->viewportBacking.g, d->viewportBacking.b, 255), col(d->border), true);
		canvasBackground_ = ofColor(d->background.r, d->background.g, d->background.b);
		hudCompositorBridge_.setLabelTypography(nullptr); // native bitmap look: no typography preset
		minimalActive_ = true;
		ofLogNotice("SystemMode") << modeId << " (background " << bg << ") — " << reason;
		return true;
	}
	// A skin package id.
	skinAuto_ = false;
	const bool ok = applySkin(modeId, reason);
	if (!ok) ofLogWarning("SystemMode") << "skin '" << modeId << "' could not be applied; keeping the current look";
	return ok;
}

void ExperienceRuntime::tickThemeFile(float dt) {
	themeCheckSeconds_ += dt;
	if (themeCheckSeconds_ < 1.0f || themePath_.empty()) return;
	themeCheckSeconds_ = 0.0f;
	std::error_code ec;
	const auto mtime = std::filesystem::last_write_time(themePath_, ec);
	if (ec || mtime == themeMtime_) return;
	loadMinimalThemes(themePath_); // records the new mtime; an invalid file keeps the previous themes
	if (minimalActive_) applySystemMode(installation_.settings().systemMode, "theme file changed");
}

void ExperienceRuntime::loadMinimalThemes(const std::string& path) {
	themePath_ = path;
	{
		std::error_code ec;
		const auto mtime = std::filesystem::last_write_time(path, ec);
		if (!ec) themeMtime_ = mtime;
	}
	if (!ofFile::doesFileExist(path, false)) return;
	ofJson j = ofLoadJson(path);
	if (!j.is_object()) { ofLogWarning("SystemMode") << "minimal theme file is not an object; defaults kept: " << path; return; }
	minimaltheme::ThemeSet t = minimaltheme::ThemeSet::defaults();
	int applied = 0;
	for (const auto g : {minimaltheme::Group::Light, minimaltheme::Group::Dark}) {
		const char* key = minimaltheme::groupKey(g);
		if (!j.contains(key) || !j[key].is_object()) continue;
		const ofJson& gj = j[key];
		if (gj.contains("primaries") && gj["primaries"].is_object()) {
			for (auto& hue : t.group(g).hues) {
				if (!gj["primaries"].contains(hue.id) || !gj["primaries"][hue.id].is_string()) continue;
				if (const auto rgb = minimaltheme::parseHex(gj["primaries"][hue.id].get<std::string>())) { hue.primary = *rgb; ++applied; }
				else ofLogWarning("SystemMode") << key << ".primaries." << hue.id << ": not a #RRGGBB colour; default kept";
			}
		}
		if (gj.contains("backgrounds") && gj["backgrounds"].is_array()) {
			std::vector<minimaltheme::Background> bgs;
			for (const auto& b : gj["backgrounds"]) {
				if (!b.is_object() || !b.contains("name") || !b["name"].is_string() || !b.contains("hex") || !b["hex"].is_string()) continue;
				if (const auto rgb = minimaltheme::parseHex(b["hex"].get<std::string>())) bgs.push_back({b["name"].get<std::string>(), *rgb});
			}
			if (!bgs.empty() && static_cast<int>(bgs.size()) <= installation::kMaxMinimalBackgrounds) { t.group(g).backgrounds = bgs; ++applied; }
		}
	}
	themes_ = t;
	ofLogNotice("SystemMode") << "minimal themes loaded from " << path << " (" << applied << " value(s) applied)";
}

void ExperienceRuntime::loadShellPalettes(const std::string& path) {
	shellPalettes_.clear();
	if (!ofFile::doesFileExist(path, false)) return;
	ofJson j;
	try { j = ofLoadJson(path); } catch (...) { j = ofJson(); }
	if (!j.is_object() || !j.contains("skins") || !j["skins"].is_object()) {
		ofLogWarning("HudSkin") << "shell palette file has no \"skins\" object; built-in palettes kept: " << path;
		return;
	}
	auto hex = [](const ofJson& o, const char* key) -> std::optional<shelltheme::Rgb> {
		if (!o.contains(key) || !o[key].is_string()) return std::nullopt;
		return shelltheme::parseHex(o[key].get<std::string>());
	};
	for (auto it = j["skins"].begin(); it != j["skins"].end(); ++it) {
		const ofJson& e = it.value();
		const auto primary = hex(e, "primary");
		const auto surface = hex(e, "surface");
		if (!e.is_object() || !primary || !surface) {
			ofLogWarning("HudSkin") << "shell palette '" << it.key() << "' needs #RRGGBB \"primary\" and \"surface\"; skipped";
			continue;
		}
		shelltheme::Seeds s;
		s.mode = e.value("mode", std::string("dark")) == "light" ? shelltheme::Mode::Light : shelltheme::Mode::Dark;
		s.primary = *primary;
		s.surface = *surface;
		s.secondary = hex(e, "secondary");
		if (e.contains("overrides") && e["overrides"].is_object()) {
			for (auto o = e["overrides"].begin(); o != e["overrides"].end(); ++o) {
				if (!o.value().is_string()) continue;
				if (const auto rgb = shelltheme::parseHex(o.value().get<std::string>())) s.overrides[o.key()] = *rgb;
				else ofLogWarning("HudSkin") << "shell palette '" << it.key() << "' override '" << o.key() << "' is not #RRGGBB; ignored";
			}
		}
		shellPalettes_[it.key()] = s;
	}
	ofLogNotice("HudSkin") << "shell palettes loaded from " << path << " (" << shellPalettes_.size() << " skin(s))";
}

int ExperienceRuntime::localMinuteNow() {
	const std::time_t now = std::time(nullptr);
	std::tm local{};
	localtime_r(&now, &local);
	return local.tm_hour * 60 + local.tm_min;
}

void ExperienceRuntime::tickSkinSchedule(bool force) {
	if (!skinAuto_) return;
	const HudSkinSchedule::Period p = skinSchedule_.periodAt(localMinuteNow());
	if (!force && skinPeriod_ && *skinPeriod_ == p) return;
	skinPeriod_ = p;
	applySkin(skinSchedule_.autoSkin(p), std::string("schedule: ") + (p == HudSkinSchedule::Period::Day ? "day" : "night"));
}

void ExperienceRuntime::update(float dt) {
	// Interactive HUD region: available only while a port exists and the active scene advertises option axes.
	hudCompositorBridge_.setInteractiveOptionAxes(regionOptionSink_ ? sceneManager_.activeOptionAxisCount() : 0);
	if (settingsPage_.isOpen()) refreshSettingsContext(false);
	else if (escapeQuitSuppressed_ && !hudCompositorBridge_.orbitEditorOpen() && !hudCompositorBridge_.interactiveActive()) { // restored one frame after closing: the closing ESC must not quit
		ofSetEscapeQuitsApp(true);
		escapeQuitSuppressed_ = false;
	}
	lastDt_ = dt; // reused by draw() (step 14) — see this member's own comment
	// Day/night skin schedule: checked about once a second (cheap; switches
	// only when the period changes, and only while following the schedule).
	skinCheckSeconds_ += dt;
	if (skinCheckSeconds_ >= 1.0f) {
		skinCheckSeconds_ = 0.0f;
		tickSkinSchedule(false);
	}
	// Sun/moon clock icon follows the real day/night state (the schedule's clock), not the theme.
	hudCompositorBridge_.setDayPeriod(skinSchedule_.periodAt(localMinuteNow()) == HudSkinSchedule::Period::Day);
	tickThemeFile(dt);
	tickInstallation(dt); // installation requests/automation, before the frame's scene work

	// Step 1 (resolve raw input) and step 2 (dispatch accepted
	// RuntimeCommand/SceneCommand values) already happened synchronously
	// in keyPressed(), which openFrameworks calls for any key event
	// before this frame's update() — adapting the task's single combined
	// per-frame pseudocode into OF's idiomatic separate event/update/draw
	// callbacks, same as the original ExperienceRuntime skeleton did.

	// Step 3 (RT-003): advance any scene transition by one frame. Performs
	// that frame's lifecycle calls (incoming setup()/activate(), capability
	// caching) and reports whether the active scene runs live this frame.
	bool live;
	{ RT004_SCOPE("SceneManager.beginFrame"); live = sceneManager_.beginFrame(); }
	applyMediaCadence(live); // before the scene/video updates below read or advance media

	if (live) {
		{ RT004_SCOPE("SceneManager.updateActiveScene"); sceneManager_.updateActiveScene(dt); } // step 3

		{ RT004_SCOPE("SceneManager.captureSceneStatus"); sceneManager_.captureSceneStatus(); }  // step 4 — hudStatus() pulled
		                                      // exactly once for this frame;
		                                      // nothing else may call it again
		                                      // until the next updateActiveScene().
	}
	// Transition-only static frames (FadingOut/Loading/Failed) neither update
	// any scene nor pull its status — SceneManager publishes the frozen
	// outgoing snapshot (FadingOut) or a neutral transition status instead.

	// step 5: SceneManager::activeCapabilities() is a cached getter, not
	// a poll — deliberately not called again here; it's read directly
	// during HudFrameData assembly in draw().

	lastSceneManagerStatus_ = sceneManager_.status(); // step 6

	{ RT004_SCOPE("RuntimeServices.update"); runtimeServices_.update(dt); } // step 7 — also drives step 8's VideoPlaybackStatus

	{ RT004_SCOPE("SceneManager.captureEffectActivityStatus"); sceneManager_.captureEffectActivityStatus(); } // step 9 — a separate pull from
	                                              // step 4; never derived from
	                                              // SceneHudStatus::activeEffects.
}

void ExperienceRuntime::reallocateSceneFboForActiveSceneIfNeeded() {
	glm::ivec2 size = sceneManager_.activeSceneNativeRenderSize();
	if (sceneFbo_.isAllocated() && static_cast<int>(sceneFbo_.getWidth()) == size.x
		&& static_cast<int>(sceneFbo_.getHeight()) == size.y) {
		return; // same native size: keep the existing allocation
	}
	// Drop the published reference before the texture behind it is
	// replaced; the new SceneFrame is built below only after the new FBO is
	// allocated and drawn.
	currentHudFrameData_.sceneFrame = SceneFrame{};
	sceneFbo_.allocate(size.x, size.y, GL_RGBA);
	presentationCounters_.sceneFboAllocations++;
	presentationCounters_.sceneFboSwitchReallocations++;
	ofLogNotice("ExperienceRuntime") << "scene FBO reallocated to " << size.x << "x" << size.y
									 << " for incoming scene \"" << lastSceneManagerStatus_.activeSceneId << "\"";
}

namespace {
// RT-008 diagnostic switches (off by default; read every frame so the RT-004
// ab-compact driver can toggle them in-process). Measurement only — they
// change what is rendered and must never ship as an optimization.
//   EXPERIENCE_RUNTIME_DIAG_SCENE_CONTROL=clear  scene FBO cleared, active
//     scene not drawn (scene/video update() still run; draw-time work such as
//     Temporal's lazy playhead uploads is skipped with the draw).
//   EXPERIENCE_RUNTIME_DIAG_HUD_OMIT=1  HUD update/draw skipped; the window
//     shows only the background.
bool diagSwitchOn(const char* name, const char* value, bool& last) {
	const char* v = std::getenv(name);
	const bool on = v != nullptr && std::string(v) == value;
	if (on != last) {
		ofLogNotice("ExperienceRuntime") << "RT-008 diagnostic " << name << " " << (on ? "ON" : "off");
		last = on;
	}
	return on;
}
}

void ExperienceRuntime::draw() {
	// Measurement only (EXPERIENCE_RUNTIME_SCENE_COLOR_STATS=1): mean/max CPU time of the whole runtime draw()
	// call, logged every 600 frames, so enabled-vs-disabled scene-colour cost can be compared under
	// identical conditions (vsync-bound frame rate hides a few ms; CPU draw time does not).
	struct DrawTimer {
		bool on; uint64_t t0;
		explicit DrawTimer(bool o) : on(o), t0(o ? ofGetElapsedTimeMicros() : 0) {}
		~DrawTimer() {
			if (!on) return;
			static double sum = 0, mx = 0; static int n = 0;
			const double us = static_cast<double>(ofGetElapsedTimeMicros() - t0);
			sum += us; mx = std::max(mx, us);
			if (++n == 600) {
				ofLogNotice("DrawCost") << "frames=600 meanDrawCpuUs=" << sum / 600.0 << " maxDrawCpuUs=" << mx;
				sum = mx = 0; n = 0;
			}
		}
	} drawTimer(sceneColorLogStats_);
	frameAcq_ = FrameAcquisition{}; // OC-003-F: per-frame accounting for the frame stream (observation only)
	static bool lastSceneControlClear = false;
	static bool lastHudOmit = false;
	const bool diagSceneClear = diagSwitchOn("EXPERIENCE_RUNTIME_DIAG_SCENE_CONTROL", "clear", lastSceneControlClear);
	const bool diagHudOmit = diagSwitchOn("EXPERIENCE_RUNTIME_DIAG_HUD_OMIT", "1", lastHudOmit);

	// RT-003: the incoming scene's first active frame — make the runtime-owned
	// scene FBO match its native size before it draws.
	if (sceneManager_.activationSucceededThisFrame()) {
		RT004_SCOPE("ExperienceRuntime.reallocateSceneFbo");
		reallocateSceneFboForActiveSceneIfNeeded();
	}
	// Orbit Console Scene Sampling: a new activation is a new scene
	// generation (history cleared, in-flight readback dropped).
	// The startup scene is activated in setup(), not through
	// activationSucceededThisFrame(), so generation 1 is begun lazily on the
	// first frame that has an active scene identity.
	if (sceneManager_.activationSucceededThisFrame()
		|| (sceneColorGeneration_ == 0 && !lastSceneManagerStatus_.activeSceneId.empty())) {
		sceneColorGeneration_++;
		sceneColorSceneId_ = lastSceneManagerStatus_.activeSceneId;
		if (sceneColorLogStats_) {
			ofLogNotice("SceneColor") << "generation " << sceneColorGeneration_ << " begins: scene=" << sceneColorSceneId_
				<< " (previous generation held " << sceneColor_.series().size() << " samples; cleared)";
		}
		sceneColor_.beginGeneration(sceneColorGeneration_, steadySeconds());
		sceneActivityProbe_.beginGeneration(sceneColorGeneration_, sceneColorSceneId_, steadySeconds()); // OC-004
	}

	if (sceneManager_.activeSceneLiveThisFrame()) {
		// Step 8: bind + clear the scene FBO, sized to the active scene's
		// nativeRenderSize().
		RT004_SCOPE("ExperienceRuntime.sceneFboPass");
		sceneFbo_.begin();
		ofClear(0, 0, 0, 0);

		// Steps 9-10: scene draws to whatever's currently bound (the
		// just-bound SceneFbo), wrapped by SceneRenderGuard so the approved
		// GL baseline (Scene-HUD-Contract-v1.md §5) is restored afterward
		// regardless of what the scene left behind. Drawing must not trigger
		// another status pull — drawActiveScene() only calls
		// drawToCurrentTarget(), never hudStatus().
		if (!diagSceneClear) {
			SceneRenderGuard guard(sceneFbo_);
			RT004_SCOPE("SceneManager.drawActiveScene");
			sceneManager_.drawActiveScene();
		}

		// Step 11: unbind SceneFbo.
		sceneFbo_.end();
		presentationCounters_.liveSceneFrames++;
		sceneActivityProbe_.onLiveFrame(sceneFbo_.getTexture(), steadySeconds()); // OC-004, inert unless opted in
		// Sample the rendered SceneFBO before HUD composition (cadence-gated).
		if (sceneColorEnabled_) {
			RT004_SCOPE("ExperienceRuntime.sceneColorSample");
			sceneColor_.onLiveFrame(sceneFbo_.getTexture(), steadySeconds());
			sceneColor_.diagFrame(sceneFbo_.getTexture(), steadySeconds(), sceneColorSceneId_, sceneColorGeneration_); // OC-003A diagnostic, inert unless EXPERIENCE_RUNTIME_SCENE_COLOR_PREDOM is set
		}
	} else {
		// RT-003 static-frame transition: no scene is asked to render. The
		// runtime-owned scene FBO still holds the outgoing scene's last live
		// frame (nothing has drawn into it since), which is exactly the
		// approved static outgoing content — retained in runtime-owned
		// state, never a pointer into a scene-owned texture/FBO, and never
		// a dual-live-scene render.
		presentationCounters_.staticSceneFrames++;
		sceneActivityProbe_.onStaticFrame(steadySeconds()); // OC-004
		if (sceneColorEnabled_) {
			sceneColor_.onStaticFrame(steadySeconds());
			sceneColor_.diagStaticFrame(steadySeconds(), sceneColorSceneId_, sceneColorGeneration_);
		}
	}
	sceneActivityProbe_.poll(steadySeconds()); // OC-004
	if (sceneColorEnabled_) {
		sceneColor_.poll(steadySeconds());
		if (sceneColorLogStats_ && steadySeconds() - sceneColorLastLogAt_ >= 10.0) {
			sceneColorLastLogAt_ = steadySeconds();
			const auto& st = sceneColor_.stats();
			const auto* nv = sceneColor_.series().newestValid();
			ofLogNotice("SceneColor") << "stream=FX mono=" << std::fixed << std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count() << std::defaultfloat // OC-003 attribution: aligns with the frame CSV
				<< " effMode=" << (sceneColor_.effectiveMode() == scenecolorsampling::ReadbackMode::Sync ? "sync" : sceneColor_.effectiveMode() == scenecolorsampling::ReadbackMode::Deferred ? "deferred" : "async") << " gpuLinear=" << (sceneColor_.gpuLinearPath() ? 1 : 0)
				<< " scene=" << sceneColorSceneId_ << " gen=" << sceneColorGeneration_
				<< " mode=" << (sceneColor_.mode() == scenecolorsampling::ReadbackMode::Sync ? "sync" : sceneColor_.mode() == scenecolorsampling::ReadbackMode::Deferred ? "deferred" : "async")
				<< " issued=" << st.issued << " done=" << st.completed << " gaps=" << st.gaps
				<< " busy=" << st.droppedBusy << " dropGen=" << st.droppedGeneration
				<< " issueAvgMs=" << (st.issued ? st.issueMsTotal / st.issued : 0.0) << " issueMaxMs=" << st.issueMsMax
				<< " collectMaxMs=" << st.collectMsMax
				<< " unsupported=" << st.unsupportedFormat << " snapshotRebuilds=" << sceneColorPublisher_.rebuilds()
				<< " rebuildMsAvg=" << (sceneColorPublisher_.rebuilds() ? sceneColorPublisher_.rebuildMsTotal() / sceneColorPublisher_.rebuilds() : 0.0)
				<< " rebuildMsMax=" << sceneColorPublisher_.rebuildMsMax()
				<< " stageMs[p50/p95/max] submit=" << st.submit.pct(0.5) << "/" << st.submit.pct(0.95) << "/" << st.submit.max
				<< " wait=" << st.wait.pct(0.5) << "/" << st.wait.pct(0.95) << "/" << st.wait.max
				<< " convert=" << st.convert.pct(0.5) << "/" << st.convert.pct(0.95) << "/" << st.convert.max
				<< " push=" << st.push.pct(0.5) << "/" << st.push.pct(0.95) << "/" << st.push.max
				<< " total=" << st.total.pct(0.5) << "/" << st.total.pct(0.95) << "/" << st.total.max
				<< " defer=" << sceneColor_.deferStrategy() << "/" << sceneColor_.deferFrames() << " latencyMs[p50/p95/max]=" << st.latency.pct(0.5) << "/" << st.latency.pct(0.95) << "/" << st.latency.max
				<< " notReady=" << st.notReadyPolls << " discardTimeout=" << st.discardedTimeout << " discardWaitFailed=" << st.discardedWaitFailed // OC-003 / A7 (no blocking fallback)
				<< " readbacks=" << st.gpuReadbacks << " readbackBytes=" << st.gpuReadbackBytes << " predominant=" << (sceneColor_.predominantActive() ? 1 : 0)
				<< " predCpuMs[p50/p95/max]=" << st.predominantCpu.pct(0.5) << "/" << st.predominantCpu.pct(0.95) << "/" << st.predominantCpu.max
				<< " " << sceneColor_.readbackSummary() << " domValid=" << st.domValid << " vibValid=" << st.vibValid << " vibInvalid=" << st.vibInvalid
				<< " invalid=" << st.invalidSamples << " telemetryDisabled=" << (sceneColor_.telemetryDisabled() ? 1 : 0) << " disabledSkips=" << st.telemetryDisabledSkips
				<< " rgb(linear)=" << (nv ? nv->r : -1.f) << "," << (nv ? nv->g : -1.f) << "," << (nv ? nv->b : -1.f)
				<< " luma=" << (nv ? nv->luma : -1.f);
		}
	}
	sampleRawVideo(); // OC-003B RAW VIDEO (borrowed decoder texture), inert unless EXPERIENCE_RUNTIME_VIDEO_COLOR=1
	sampleCpuRaw(); // OC-003-F CPU RAW prototype (borrowed decoder pixels), inert unless EXPERIENCE_RUNTIME_CPU_RAW=1
	if (paletteOn_) frameAcq_.palUs += sceneColor_.takePaletteUs() + videoColor_.takePaletteUs();
	frameAcq_.videoNew = runtimeServices_.video().isFrameNew();
	frameNumber_++; // monotonically increasing, once per presented frame

	// Step 12: construct this frame's read-only SceneFrame.
	SceneFrame frame;
	frame.texture = &sceneFbo_.getTexture();
	frame.nativeSize = glm::ivec2(sceneFbo_.getWidth(), sceneFbo_.getHeight());
	frame.frameNumber = frameNumber_;

	// Step 13: assemble HudFrameData — ExperienceRuntime alone does this;
	// it is never assembled inside IEcopunkScene, SceneManager, or
	// RuntimeServices. All fields are value snapshots except
	// sceneFrame.texture (the one intentional non-owning render
	// reference, into sceneFbo_, which outlives this frame).
	currentHudFrameData_.schemaVersion = 1;
	currentHudFrameData_.sceneFrame = frame;
	currentHudFrameData_.scene = sceneManager_.currentSceneStatus(); // the frame-4-captured status
	currentHudFrameData_.sceneManager = lastSceneManagerStatus_;      // captured in update(), step 6
	currentHudFrameData_.capabilities = sceneManager_.activeCapabilities(); // cached, not re-polled
	currentHudFrameData_.runtime = runtimeServices_.telemetry();
	// System Telemetry v1: shared_ptr copy only (refcount bump) — the
	// immutable snapshot is rebuilt in RuntimeServices::update, never here.
	currentHudFrameData_.systemVitals = runtimeServices_.systemVitalsSnapshot();
	// Orbit Console v0.1: the runtime-owned scene-colour snapshot (sampled from the SceneFBO above,
	// before HUD composition). Republished only when the series revision / state / scene changes, so
	// this is a shared_ptr copy per frame. Null while disabled or before the first scene activation.
	// OC-003B: RAW VIDEO series (null unless EXPERIENCE_RUNTIME_VIDEO_COLOR=1). Rebuilt only on a RAW sample/gap,
	// generation, state or media change (SnapshotPublisher), otherwise the same shared_ptr: a refcount bump.
	currentHudFrameData_.sceneColorRaw = videoColorEnabled_
		? videoColorPublisher_.publish(videoColor_.series(), steadySeconds(), videoColorMediaId_, videoColor_.predominantActive())
		: nullptr;
	// OC-004: scene visual activity (scene.visual.activity). Null unless EXPERIENCE_RUNTIME_SCENE_VISUAL_ACTIVITY=1
	// (production default OFF). Rebuilt only on a new observation / state / generation change.
	currentHudFrameData_.sceneVisualActivity = sceneActivityProbe_.publishing()
		? sceneVisualActivityPublisher_.publish(sceneActivityProbe_.tracker(), steadySeconds())
		: nullptr;
	{ // Verification only (EXPERIENCE_RUNTIME_SCENE_VISUAL_ACTIVITY_LOG=1): the published snapshot, ~4 Hz.
		static const bool logOn = [] { const char* v = std::getenv("EXPERIENCE_RUNTIME_SCENE_VISUAL_ACTIVITY_LOG"); return v && std::string(v) == "1"; }();
		static double nextLogAt = 0.0;
		const auto& vs = currentHudFrameData_.sceneVisualActivity;
		if (logOn && steadySeconds() >= nextLogAt) {
			nextLogAt = steadySeconds() + 0.25;
			static const char* names[] = {"unavailable", "valid", "stale", "transition"};
			ofLogNotice("VisualActivityPub") << "t=" << steadySeconds() << " " << (vs ? "snapshot" : "null")
				<< " scene=" << (vs ? vs->sceneId : std::string("-")) << " gen=" << (vs ? vs->generation : 0)
				<< " state=" << (vs ? names[static_cast<int>(vs->state)] : "none") << " raw=" << (vs ? vs->raw : 0.0f)
				<< " level=" << (vs ? vs->level : 0.0f) << " rev=" << (vs ? vs->revision : 0);
		}
	}
	currentHudFrameData_.sceneColor = sceneColorEnabled_
		? sceneColorPublisher_.publish(sceneColor_.series(), steadySeconds(), sceneColorSceneId_, sceneColor_.predominantActive())
		: nullptr;
	// Captured once here, from RuntimeServices' already-updated (step 7)
	// VideoPlaybackService — never polled again during the HUD draw call
	// below, matching this same rule for every other HudFrameData field
	// (see this class's own header comment, step 13).
	currentHudFrameData_.video = runtimeServices_.videoPlaybackStatus();
	// Captured in update() (step 9) — the authoritative Shared Effects
	// snapshot, transported unchanged. std::nullopt here means "no
	// snapshot available," not "zero effects" — see HudFrameData.h.
	currentHudFrameData_.effects = sceneManager_.currentEffectActivityStatus();
	// HUD Clock v0.1: runtime-owned OS wall clock — one coherent local-time
	// conversion per distinct second (see WallClockSampler.h), never a scene's.
	currentHudFrameData_.wallClock = wallClockSampler_.sample(std::chrono::system_clock::now());
	if (installationLoaded_) currentHudFrameData_.installation = buildInstallationStatus();
	if (logCapture_) currentHudFrameData_.activityLog = logCapture_->recent();
	presentationCounters_.hudFrameDataAssemblies++;

	// Step 16: hand the ONE assembled HudFrameData to the compositor
	// bridge, by const& — it never receives the FBO, and never polls
	// SceneManager/scenes/RuntimeServices itself. Engineering Session 2:
	// this is now the real, shared hudpresent::HudWireframeRenderer (via
	// HudCompositorBridge, not HudCompositorStub's plain unclipped
	// rectangle + debug text) — canonical 1280x720 layout, MediaViewportMesh,
	// widgets, drawn at the canvas origin (no destRect: see
	// HudCompositorBridge's header comment for why none is needed).
	// RT-007: the final presentation transform for THIS frame, from the
	// actual framebuffer size (ofGetWidth/Height are framebuffer pixels under
	// GLFW) — read every frame, so fullscreen toggles, WM/external resizes
	// and late X11 mode changes are picked up without any resize callback,
	// and never inferred from a fullscreen request having been made.
	{
		const PresentationTransform next = PresentationTransform::contain(ofGetWidth(), ofGetHeight());
		static int lastLoggedWindowMode = -1; // fullscreen can settle a few frames after the size does
		const int windowMode = static_cast<int>(ofGetWindowMode());
		if (next != presentation_ || frameNumber_ == 1 || windowMode != lastLoggedWindowMode) {
			lastLoggedWindowMode = windowMode;
			ofLogNotice("Presentation") << "framebuffer " << next.framebufferWidth << "x" << next.framebufferHeight
				<< " (" << (ofGetWindowMode() == OF_FULLSCREEN ? "fullscreen" : "windowed") << ") -> "
				<< (next.isIdentity() ? std::string("direct 1280x720") : "canonical 1280x720 composition scaled "
					+ ofToString(next.scale, 4) + " at (" + ofToString(next.offsetX, 1) + ", " + ofToString(next.offsetY, 1) + ")");
		}
		presentation_ = next;
	}
	// Architecture-Closure Session: HUD-only allocation isolation — see
	// this class's own header comment (lastHudOnlyNewCount()/
	// lastHudOnlyDeleteCount()). Reads alloccounter's global counters
	// immediately around ONLY these two calls; zero added cost when
	// alloccounter::g_enabled is false (the atomic loads are cheap, and
	// nothing else changes).
	long long hudAllocBefore = alloccounter::newCount();
	long long hudDeallocBefore = alloccounter::deleteCount();
	if (!diagHudOmit) {
		{ RT004_SCOPE("Hud.update"); hudCompositorBridge_.update(lastDt_, currentHudFrameData_); }
		{ // Verification only (EXPERIENCE_RUNTIME_SCENE_VISUAL_ACTIVITY_LOG=1): snapshot + the region #6 gauge input, on change and at ~4 Hz.
			static const bool logOn = [] { const char* v = std::getenv("EXPERIENCE_RUNTIME_SCENE_VISUAL_ACTIVITY_LOG"); return v && std::string(v) == "1"; }();
			static std::string lastKey; static double nextAt = 0.0;
			if (logOn) {
				const auto& vs = currentHudFrameData_.sceneVisualActivity;
				const auto* in = hudCompositorBridge_.renderer().widgetInputFor("universal.activity.bar");
				const auto* gv = in ? in->findRole("value") : nullptr;
				static const char* names[] = {"unavailable", "valid", "stale", "transition"};
				const char* gauge = !gv || !gv->shouldRender ? "hidden" : gv->useAmbientFallback ? "ambient" : (gv->value.dataClass == hudpresent::HudDataClass::Ambient ? "placeholder" : "value");
				const float gval = gv && gv->shouldRender && !gv->useAmbientFallback && gv->value.dataClass != hudpresent::HudDataClass::Ambient ? gv->value.numberValue : -1.0f;
				const std::string key = std::string(vs ? names[static_cast<int>(vs->state)] : "none") + "/" + (vs ? std::to_string(vs->generation) : "0") + "/" + gauge;
				if (key != lastKey || steadySeconds() >= nextAt) {
					lastKey = key; nextAt = steadySeconds() + 0.25;
					ofLogNotice("VisualActivityHud") << "t=" << steadySeconds() << " live=" << (sceneManager_.activeSceneLiveThisFrame() ? 1 : 0)
						<< " scene=" << (vs ? vs->sceneId : std::string("-")) << " gen=" << (vs ? vs->generation : 0)
						<< " state=" << (vs ? names[static_cast<int>(vs->state)] : "none") << " raw=" << (vs ? vs->raw : 0.0f) << " level=" << (vs ? vs->level : 0.0f)
						<< " gauge=" << gauge << " gaugeValue=" << gval << " rev=" << (vs ? vs->revision : 0);
				}
			}
		}
		if (immersive_) {
			// IMMERSIVE (HUD & Scene Experience v0.3): the HUD keeps updating
			// (focus/status stay current) but is not drawn; the runtime-owned
			// scene FBO fills the CURRENT framebuffer instead — aspect-
			// preserving cover scale, centred crop. No window/fullscreen
			// change, no per-scene rendering. Independent of RT-007's
			// contain transform (scene-only presentation, by design).
			RT004_SCOPE("Immersive.draw");
			drawImmersiveScene();
		} else if (presentation_.isIdentity()) {
			// 1280x720 framebuffer: draw the canonical composition directly,
			// exactly as before RT-007 (no extra target, no extra pass).
			RT004_SCOPE("Hud.draw");
			ofBackground(canvasBackground_);
			drawComposition();
			presentationCounters_.directPresentationFrames++;
		} else {
			// Any other framebuffer: compose once at the canonical 1280x720
			// (shell, widgets, native-size bitmap text and the beveled
			// MediaViewportMesh all keep their 1:1 pixel layout), then scale
			// the finished image once. A plain ofScale() around the HUD is NOT
			// equivalent: OF's default bitmap-string mode billboards only the
			// translation and draws glyphs at native size, so text would
			// shrink relative to the shell.
			RT004_SCOPE("Hud.drawComposed");
			if (!presentationFbo_.isAllocated()) {
				presentationFbo_.allocate(static_cast<int>(PresentationTransform::kCanvasWidth),
					static_cast<int>(PresentationTransform::kCanvasHeight), GL_RGBA);
				presentationFbo_.getTexture().setTextureMinMagFilter(GL_LINEAR, GL_LINEAR);
				presentationCounters_.presentationFboAllocations++;
				ofLogNotice("Presentation") << "canonical composition FBO allocated 1280x720 RGBA (once)";
			}
			presentationFbo_.begin();
			ofClear(canvasBackground_.r, canvasBackground_.g, canvasBackground_.b, 255);
			drawComposition();
			presentationFbo_.end();
			presentComposedFrame();
			presentationCounters_.composedPresentationFrames++;
		}
	} else {
		ofBackground(canvasBackground_);
	}
	lastHudOnlyNewCount_ = alloccounter::newCount() - hudAllocBefore;
	lastHudOnlyDeleteCount_ = alloccounter::deleteCount() - hudDeallocBefore;

	// Step 15: present — openFrameworks swaps buffers after draw() returns.
}

// OC-003B: RAW VIDEO sampling. Runs once per frame on the main GL thread, after the service update and
// before any further playback operation, so the borrowed VideoPlaybackService::currentTexture() is the one
// the decoder just produced. The texture is re-acquired here every frame and never cached. FX is untouched.
void ExperienceRuntime::sampleRawVideo() {
	if (!videoColorEnabled_) return;
	RT004_SCOPE("ExperienceRuntime.videoColorSample");
	const double now = steadySeconds();
	const VideoPlaybackService& video = runtimeServices_.video();
	const auto status = runtimeServices_.videoPlaybackStatus();
	const ofTexture* tex = video.currentTexture();
	const glm::ivec2 src = video.sourceSize();

	rawvideo::Observation o;
	o.healthReady = status && status->health == VideoPlaybackHealth::Ready;
	o.hasMediaId = status && status->mediaId.has_value();
	o.textureAllocated = tex != nullptr && tex->isAllocated();
	if (o.textureAllocated) { o.texW = static_cast<int>(tex->getWidth()); o.texH = static_cast<int>(tex->getHeight()); }
	o.srcW = src.x;
	o.srcH = src.y;
	o.frameNew = video.isFrameNew();
	o.activationCount = video.activationCount();
	o.sceneGeneration = sceneColorGeneration_;
	o.nowSeconds = now;

	const rawvideo::Decision d = videoColorGate_.observe(o);
	if (d.newGeneration) {
		videoColorMediaId_ = (status && status->mediaId) ? *status->mediaId : std::string();
		// Half a period after FX's phase so the two reductions never land in the same frame.
		videoColor_.beginGeneration(d.rawGeneration, now + scenecolorsampling::kPeriodSeconds * 0.5);
		if (videoColorStats_) {
			ofLogNotice("VideoColor") << "generation " << d.rawGeneration << " begins: media=" << (status && status->mediaId ? *status->mediaId : std::string("-"))
				<< " activation=" << o.activationCount << " scene_gen=" << o.sceneGeneration << " verdict=" << static_cast<int>(d.verdict);
		}
	}
	if (d.verdict == rawvideo::Verdict::Eligible) {
		videoColor_.onLiveFrame(*tex, now);
	} else if (d.verdict == rawvideo::Verdict::Gap) {
		videoColor_.onStaticFrame(now); // a due sample becomes an explicit gap, never a guess
	}
	videoColor_.poll(now);

	if (videoColorDiag_ && d.verdict == rawvideo::Verdict::Eligible && videoColor_.series().revision() != videoColorLastDiagRev_ && videoColor_.series().size() > 0) {
		videoColorLastDiagRev_ = videoColor_.series().revision();
		const auto& td = tex->getTextureData();
		const scenecolorsampling::Sample& m = videoColor_.series().at(videoColor_.series().size() - 1);
		float rr = 0, rg = 0, rb = 0;
		bool haveRef = false;
		if (const ofPixels* px = video.currentPixels()) { // CPU reference: the decoder's own pixels, independent of the GL texture
			const size_t n = static_cast<size_t>(px->getWidth()) * px->getHeight();
			const size_t ch = px->getNumChannels();
			if ((ch == 3 || ch == 4) && n > 0) {
				const auto& lut = scenecolorsampling::srgbLut();
				double sr = 0, sg = 0, sb = 0;
				const uint8_t* p = px->getData();
				for (size_t i = 0; i < n; ++i) { sr += lut[p[i * ch]]; sg += lut[p[i * ch + 1]]; sb += lut[p[i * ch + 2]]; }
				rr = static_cast<float>(sr / n); rg = static_cast<float>(sg / n); rb = static_cast<float>(sb / n);
				haveRef = true;
			}
		}
		ofLogNotice("VideoColorDiag") << "tex target=0x" << std::hex << td.textureTarget << " internalFormat=0x" << td.glInternalFormat << std::dec
			<< " id=" << td.textureID << " " << o.texW << "x" << o.texH << " src=" << o.srcW << "x" << o.srcH
			<< " gen=" << d.rawGeneration << " valid=" << m.valid << " Y=" << m.luma << " rgb=" << m.r << "," << m.g << "," << m.b
			<< (haveRef ? " refY=" + ofToString(scenecolorsampling::luma709(rr, rg, rb), 5) + " dY=" + ofToString(m.luma - scenecolorsampling::luma709(rr, rg, rb), 5) : std::string(" ref=n/a"));
	}
	if (videoColorStats_ && now - videoColorLastLogAt_ >= 10.0) {
		videoColorLastLogAt_ = now;
		const auto& st = videoColor_.stats();
		ofLogNotice("VideoColor") << "stream=RAW mono=" << std::fixed << std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count() << std::defaultfloat // OC-003 attribution
			<< " effMode=" << (videoColor_.effectiveMode() == scenecolorsampling::ReadbackMode::Sync ? "sync" : videoColor_.effectiveMode() == scenecolorsampling::ReadbackMode::Deferred ? "deferred" : "async") << " gpuLinear=" << (videoColor_.gpuLinearPath() ? 1 : 0)
			<< " notReady=" << st.notReadyPolls << " discardTimeout=" << st.discardedTimeout << " discardWaitFailed=" << st.discardedWaitFailed << " invalid=" << st.invalidSamples
			<< " latencyMs[p50/p95/max]=" << st.latency.pct(0.5) << "/" << st.latency.pct(0.95) << "/" << st.latency.max
			<< " gen=" << d.rawGeneration << " gate=" << static_cast<int>(d.verdict) << " issued=" << st.issued << " completed=" << st.completed
			<< " droppedGen=" << st.droppedGeneration << " droppedBusy=" << st.droppedBusy << " gaps=" << st.gaps << " unsupported=" << st.unsupportedFormat
			<< " stageMs[p50/p95/max] submit=" << st.submit.pct(0.5) << "/" << st.submit.pct(0.95) << "/" << st.submit.max
			<< " wait=" << st.wait.pct(0.5) << "/" << st.wait.pct(0.95) << "/" << st.wait.max
			<< " convert=" << st.convert.pct(0.5) << "/" << st.convert.pct(0.95) << "/" << st.convert.max
			<< " total=" << st.total.pct(0.5) << "/" << st.total.pct(0.95) << "/" << st.total.max
			<< " readbacks=" << st.gpuReadbacks << " readbackBytes=" << st.gpuReadbackBytes << " predominant=" << (videoColor_.predominantActive() ? 1 : 0)
			<< " predCpuMs[p50/p95/max]=" << st.predominantCpu.pct(0.5) << "/" << st.predominantCpu.pct(0.95) << "/" << st.predominantCpu.max
			<< " " << videoColor_.readbackSummary() << " domValid=" << st.domValid << " vibValid=" << st.vibValid << " vibInvalid=" << st.vibInvalid;
	}
}

// OC-003-F: CPU RAW prototype. Consumes VideoPlaybackService::currentPixels() -- the decoder's existing CPU-resident frame -- on the
// main thread in the same frame as the decoder update, reduces it to the 160x90 linear-light grid (cpuraw::reduceToGrid, same arithmetic
// as the GPU RAW shader) and derives AVG/DOM/VIB with the production code. No GPU work, no second decoder, nothing published to the HUD.
// Eligibility / generations use a SEPARATE rawvideo::Gate with the same rules as the GPU RAW gate. Pixels are only requested when a
// sample is due (on macOS ofAVFoundationPlayer::getPixels converts lazily, so asking every frame would add cost that is not acquisition).
void ExperienceRuntime::sampleCpuRaw() {
	if (!cpuRawEnabled_) return;
	RT004_SCOPE("ExperienceRuntime.cpuRawSample");
	const double now = steadySeconds();
	const VideoPlaybackService& video = runtimeServices_.video();
	const auto status = runtimeServices_.videoPlaybackStatus();
	const ofTexture* tex = video.currentTexture(); // eligibility only (cheap, identical rule to the GPU RAW gate); pixels are read below
	const glm::ivec2 src = video.sourceSize();

	rawvideo::Observation o;
	o.healthReady = status && status->health == VideoPlaybackHealth::Ready;
	o.hasMediaId = status && status->mediaId.has_value();
	o.textureAllocated = tex != nullptr && tex->isAllocated();
	if (o.textureAllocated) { o.texW = static_cast<int>(tex->getWidth()); o.texH = static_cast<int>(tex->getHeight()); }
	o.srcW = src.x;
	o.srcH = src.y;
	o.frameNew = video.isFrameNew();
	o.activationCount = video.activationCount();
	o.sceneGeneration = sceneColorGeneration_;
	o.nowSeconds = now;

	const rawvideo::Decision d = cpuRawGate_.observe(o);
	if (d.newGeneration) {
		// Half a period after the FX phase, exactly like the GPU RAW sampler; history, tracker and palette temporal state restart.
		cpuRaw_.beginGeneration(d.rawGeneration, now + scenecolorsampling::kPeriodSeconds * 0.5);
		if (paletteOn_) paletteSink_.gap(palette::kRawCpu);
		if (cpuRawStats_) ofLogNotice("CpuRaw") << "generation " << d.rawGeneration << " begins: media=" << (status && status->mediaId ? *status->mediaId : std::string("-"))
			<< " activation=" << o.activationCount << " scene_gen=" << o.sceneGeneration << " verdict=" << static_cast<int>(d.verdict);
	}
	if (d.verdict == rawvideo::Verdict::Eligible && cpuRaw_.due(now)) {
		const double a0 = cpuraw::wallMs(), ac0 = cpuraw::threadCpuMs();
		const ofPixels* px = video.currentPixels();
		const double accessWall = cpuraw::wallMs() - a0;
		const double ac1 = cpuraw::threadCpuMs();
		const double accessCpu = (ac0 >= 0 && ac1 >= 0) ? ac1 - ac0 : -1.0;
		cpuraw::PixelView v;
		if (px) {
			v.data = px->getData();
			v.w = static_cast<int>(px->getWidth());
			v.h = static_cast<int>(px->getHeight());
			const auto fmt = px->getPixelFormat();
			// Only packed 8-bit RGB / RGBA are accepted; anything else (BGR, planar, 16-bit) is refused and counted, never reinterpreted.
			v.channels = (px->getBytesPerChannel() == 1 && (fmt == OF_PIXELS_RGB || fmt == OF_PIXELS_RGBA)) ? static_cast<int>(px->getNumChannels()) : 0;
			v.rowBytes = static_cast<std::size_t>(v.w) * static_cast<std::size_t>(px->getNumChannels());
		}
		const auto out = cpuRaw_.onFrame(v, src.x, src.y, now, accessWall, accessCpu);
		if (out == cpuraw::Sampler::Outcome::Sampled) {
			frameAcq_.cpuRaw++;
			frameAcq_.cpuRawUs = cpuRaw_.lastTotalWallMs() * 1000.0;
			if (paletteOn_) frameAcq_.palUs += paletteSink_.add(palette::kRawCpu, cpuRaw_.series().generation(), now, cpuRaw_.grid(), cpuRaw_.last());
		} else if (out == cpuraw::Sampler::Outcome::Gap) {
			frameAcq_.cpuRawGap++;
			frameAcq_.cpuRawUs = cpuRaw_.lastTotalWallMs() * 1000.0;
			if (paletteOn_) paletteSink_.gap(palette::kRawCpu);
		}
	} else if (d.verdict == rawvideo::Verdict::Gap) {
		if (cpuRaw_.recordGap(now) == cpuraw::Sampler::Outcome::Gap) { frameAcq_.cpuRawGap++; if (paletteOn_) paletteSink_.gap(palette::kRawCpu); }
	}

	if (cpuRawStats_ && now - cpuRawLastLogAt_ >= 10.0) {
		cpuRawLastLogAt_ = now;
		const cpuraw::Stats& st = cpuRaw_.stats();
		auto tri = [](const cpuraw::Dist& x) { std::ostringstream os; os << x.pct(0.5) << "/" << x.pct(0.95) << "/" << x.max(); return os.str(); };
		ofLogNotice("CpuRaw") << "stream=CPURAW mono=" << std::fixed << std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count() << std::defaultfloat
			<< " gen=" << d.rawGeneration << " gate=" << static_cast<int>(d.verdict) << " due=" << st.due << " samples=" << st.samples << " gaps=" << st.gaps
			<< " noPixels=" << st.noPixels << " badFormat=" << st.badFormat << " dimMismatch=" << st.dimMismatch << " droppedGen=" << st.droppedGeneration
			<< " domValid=" << st.domValid << " vibValid=" << st.vibValid << " vibInvalid=" << st.vibInvalid
			<< " accessWallMs[p50/p95/max]=" << tri(st.accessWall) << " accessCpuMs[p50/p95/max]=" << tri(st.accessCpu)
			<< " reduceWallMs[p50/p95/max]=" << tri(st.reduceWall) << " reduceCpuMs[p50/p95/max]=" << tri(st.reduceCpu)
			<< " analyzeWallMs[p50/p95/max]=" << tri(st.analyzeWall) << " analyzeCpuMs[p50/p95/max]=" << tri(st.analyzeCpu)
			<< " totalWallMs[p50/p95/max]=" << tri(st.totalWall) << " totalCpuMs[p50/p95/max]=" << tri(st.totalCpu);
	}
}

void ExperienceRuntime::drawComposition() {
	hudCompositorBridge_.draw();
	if (typography_) typography_->drawOverlay(); // dev-only info panel, above the HUD
	if (settingsPage_.isOpen()) drawExperienceSettingsPage(settingsPage_); // topmost: Experience Settings
	presentationCounters_.hudDraws++;
}

void ExperienceRuntime::presentComposedFrame() {
	ofBackground(0); // letterbox bars (none at 16:9 framebuffers such as 1600x900)
	ofPushStyle();
	// Copy RGB only: translucent HUD elements lowered the composition's
	// alpha inside the FBO; blending it over black would darken them.
	ofDisableAlphaBlending();
	ofSetColor(255);
	presentationFbo_.draw(presentation_.offsetX, presentation_.offsetY, presentation_.drawWidth(), presentation_.drawHeight());
	ofPopStyle();
}

void ExperienceRuntime::exit() {
	sceneColor_.shutdown(); // release sampler fences while the GL context is alive
	paletteSink_.close(); // OC-003-F: flush CSVs + palette_summary.json (no-op when closed)
	videoColor_.shutdown();
	hudCompositorBridge_.shutdownSkins(); // release skin textures while the GL context is alive
	sceneManager_.deactivateScene();
	sceneManager_.shutdown(); // every registered scene that was ever set up
	runtimeServices_.shutdown();
}

void ExperienceRuntime::handleFocusInput(const HudFocusInput& input) {
	// ENERGY adjustment borrows the same device-neutral encoder input: steps
	// change ENERGY instead of focus, the click exits. Focus is untouched.
	if (installation_.energyAdjusting()) {
		installation::Request r;
		if (input.kind == HudFocusInput::Kind::Step) {
			r.kind = installation::Request::Kind::StepEnergy;
			r.steps = input.delta;
		} else {
			r.kind = installation::Request::Kind::EndEnergyAdjust;
		}
		installation_.submit(r);
		ofLogNotice("Installation") << "ENERGY target " << ofToString(installation_.settings().energyTarget, 2)
			<< (installation_.energyAdjusting() ? " (adjusting)" : " (adjustment closed)");
		return;
	}
	// OC-005: while an Orbit observation editor is open it is the sole consumer of the device-neutral focus
	// input (step = move the cursor, activate = select / DONE). HUD focus stays on the edited observation, so
	// closing returns to it.
	if (hudCompositorBridge_.orbitEditorOpen()) {
		if (input.kind == HudFocusInput::Kind::Step) {
			hudCompositorBridge_.orbitEditorStep(input.delta);
			return;
		}
		const double now = steadySeconds();
		if (lastOkInputAt_ >= 0.0 && now - lastOkInputAt_ < 0.25) return; // debounce activate auto-repeat
		lastOkInputAt_ = now;
		const hudpresent::OrbitObs editedObs = hudCompositorBridge_.orbitUi().target();
		const bool closed = hudCompositorBridge_.orbitEditorActivate();
		if (closed) {
			orbitEditorClosedAt_ = now;
			commitOrbitObservation(editedObs); // Done = commit + persist (only the edited observation)
		}
		const auto& oc = hudCompositorBridge_.orbitUi().config();
		ofLogNotice("OrbitEditor") << (closed ? "done (committed)" : "selected") << ": A=" << hudpresent::orbitSourceText(oc.a.source) << "/"
			<< hudpresent::orbitMetricText(oc.a.metric) << " B=" << hudpresent::orbitSourceText(oc.b.source) << "/" << hudpresent::orbitMetricText(oc.b.metric);
		return;
	}
	// Interactive region (encoder path): rotation = step the selected row's value; click = next row,
	// then leave (so a one-axis encoder is never trapped). Keyboard Enter is handled earlier in keyPressed().
	if (hudCompositorBridge_.interactiveActive()) {
		if (input.kind == HudFocusInput::Kind::Step) {
			stepInteractiveOption(input.delta);
		} else if (hudCompositorBridge_.interactiveAdvanceRowOrExit()) {
			interactiveClosedAt_ = steadySeconds();
		}
		return;
	}
	if (input.kind == HudFocusInput::Kind::Step) {
		hudCompositorBridge_.stepFocus(input.delta);
		const auto& f = hudCompositorBridge_.focus().focused();
		ofLogNotice("HudFocus") << "focus -> " << (f ? *f : std::string("none (no visible controls)"));
		return;
	}

	// The Enter that closed Settings (or its key-repeat) must not re-activate
	// the MENU control the HUD focus returned to.
	if (settingsClosedAt_ >= 0.0 && steadySeconds() - settingsClosedAt_ < 0.6) return;
	// The Enter (or its key-repeat) that closed the Orbit editor must not re-open it.
	if (orbitEditorClosedAt_ >= 0.0 && steadySeconds() - orbitEditorClosedAt_ < 0.6) return;
	// The Enter that closed the interactive region must not re-enter it.
	if (interactiveClosedAt_ >= 0.0 && steadySeconds() - interactiveClosedAt_ < 0.6) return;
	// Activate: only an ENABLED focused control dispatches, and only through
	// the existing command paths — the same calls the 'n'/'p'/'['/']' keys use.
	FocusActivationRecord rec;
	rec.serial = lastFocusActivation_.serial + 1;
	const auto& focused = hudCompositorBridge_.focus().focused();
	rec.region = focused ? *focused : std::string();
	// Activating whatever holds focus (even a fallback, even if refused) is
	// deliberate: it becomes the intended control, ending any restoration.
	if (focused) hudCompositorBridge_.confirmFocus();
	const std::optional<std::string> target = hudCompositorBridge_.focusActivationTarget();
	if (!target) {
		rec.refused = focused.has_value(); // focused but disabled; no focus = nothing to do
		if (rec.refused) hudCompositorBridge_.reportActivation(false);
		ofLogNotice("HudFocus") << (rec.refused ? "refused: '" + rec.region + "' is not currently available"
		                                        : std::string("activate ignored: nothing focused"));
		lastFocusActivation_ = rec;
		return;
	}

	// Control region -> existing command. Same meanings the HUD's availability
	// sources use (HudRealFrameResolver control.* sources).
	if (HudCompositorBridge::isInteractiveFocusId(*target)) {
		// Generic interactive region: Enter captures the arrows (Up/Down = row, Left/Right = value).
		// HUD presentation state only: no scene/media/runtime command until a value step.
		const bool entered = hudCompositorBridge_.enterInteractiveForFocus();
		if (entered) {
			ofSetEscapeQuitsApp(false); // ESC leaves the region, never quits
			escapeQuitSuppressed_ = true;
		}
		rec.command = "HudInteractive::Enter";
		rec.accepted = entered;
		ofLogNotice("HudInteractive") << (entered ? "active (Up/Down = row, Left/Right = value, Enter/Esc = done)" : "refused: region unavailable");
	} else if (HudCompositorBridge::isOrbitFocusId(*target)) {
		// OC-005: an Orbit observation opens its editor. HUD presentation state only: no scene/media/runtime command.
		lastOkInputAt_ = steadySeconds();
		const bool opened = hudCompositorBridge_.openOrbitEditorForFocus();
		if (opened) {
			ofSetEscapeQuitsApp(false); // ESC cancels the editor, never quits
			escapeQuitSuppressed_ = true;
		}
		rec.command = "HudOrbit::OpenEditor";
		rec.accepted = opened;
		ofLogNotice("OrbitEditor") << (opened ? "open: editing " : "refused: ") << *target;
	} else if (*target == "controls.fullscreen") {
		// FULLSCREEN = IMMERSIVE: the scene fills the OF window viewport (HUD
		// hidden; any key/press exits). It never changes the window mode —
		// native fullscreen is key 'f' / Installation Controls.
		enterImmersive();
		rec.command = "Runtime::EnterImmersive";
		rec.accepted = true;
	} else if (*target == "controls.menu") {
		// MENU: the one established Settings entry point (same as F1 / ','); no
		// scene/media/mode command. HUD focus stays on MENU, so closing the page
		// returns to it. Key-repeat from this Enter must not hit the page's first row.
		openSettingsPage();
		rec.command = "Runtime::OpenSettings";
		rec.accepted = settingsPage_.isOpen();
	} else if (*target == "controls.mode") {
		rec.command = "SceneCommand::NextMode";
		rec.accepted = sceneManager_.dispatchSceneCommand(SceneCommand::NextMode);
	} else if (*target == "controls.media_previous" || *target == "controls.media_next") {
		const SceneCommand cmd = *target == "controls.media_previous" ? SceneCommand::PreviousMedia : SceneCommand::NextMedia;
		rec.command = cmd == SceneCommand::PreviousMedia ? "SceneCommand::PreviousMedia" : "SceneCommand::NextMedia";
		rec.accepted = sceneManager_.dispatchSceneCommand(cmd);
	} else if (*target == "controls.scene_previous" || *target == "controls.scene_next") {
		const RuntimeCommand cmd = *target == "controls.scene_previous" ? RuntimeCommand::PreviousScene : RuntimeCommand::NextScene;
		rec.command = cmd == RuntimeCommand::PreviousScene ? "RuntimeCommand::PreviousScene" : "RuntimeCommand::NextScene";
		handleRuntimeCommand(cmd); // existing path; it reports no accept/reject result
		rec.accepted = true;
	} else {
		rec.refused = true; // unknown control: never guess a command
	}
	hudCompositorBridge_.reportActivation(rec.accepted && !rec.refused);
	ofLogNotice("HudFocus") << "activate '" << rec.region << "' -> " << (rec.command.empty() ? "(none)" : rec.command)
		<< ", accepted=" << (rec.accepted && !rec.refused);
	lastFocusActivation_ = rec;
}

void ExperienceRuntime::stepInteractiveOption(int delta) {
	const int row = hudCompositorBridge_.interactiveRow();
	const bool applied = regionOptionSink_ ? regionOptionSink_(row, delta) : false; // sole exit of a value step
	ofLogNotice("HudInteractive") << "row " << row << " step " << delta << (applied ? " applied" : " refused (no option port)");
}

// Active interactive region keys. Returns true when consumed. Up/Down select the row (clamped),
// Left/Right step the selected row's value (the callee wraps), Enter/Esc leave (applied values stay).
bool ExperienceRuntime::handleInteractiveKey(int key) {
	switch (key) {
		case OF_KEY_UP:    hudCompositorBridge_.interactiveSelectRow(-1); return true;
		case OF_KEY_DOWN:  hudCompositorBridge_.interactiveSelectRow(+1); return true;
		case OF_KEY_LEFT:  stepInteractiveOption(-1); return true;
		case OF_KEY_RIGHT: stepInteractiveOption(+1); return true;
		case OF_KEY_RETURN:
		case OF_KEY_ESC:
			hudCompositorBridge_.exitInteractive();
			interactiveClosedAt_ = steadySeconds();
			ofLogNotice("HudInteractive") << "done (applied values kept)";
			return true;
		default: return false;
	}
}

void ExperienceRuntime::enterImmersive() {
	hudCompositorBridge_.orbitEditorBack(); // OC-005: the HUD is hidden while immersive; an open Orbit editor is cancelled
	hudCompositorBridge_.exitInteractive();
	immersive_ = true;
	ofLogNotice("ExperienceRuntime") << "immersive: on (any key/press exits)";
}

bool ExperienceRuntime::exitImmersiveOnInput() {
	if (!immersive_) return false;
	immersive_ = false; // the exit input is consumed — it navigates nothing
	ofLogNotice("ExperienceRuntime") << "immersive: off";
	return true;
}

void ExperienceRuntime::pointerPressed(float x, float y) {
	if (settingsPage_.isOpen()) return; // the page is keyboard/D-pad only; a press must not leak underneath
	// RT-007: framebuffer -> canonical canvas through the inverse of the
	// final presentation transform (no pointer-driven HUD control exists
	// yet; the press itself only exits IMMERSIVE).
	if (x >= 0.0f && y >= 0.0f) {
		lastPointerCanvas_ = {presentation_.toCanvasX(x), presentation_.toCanvasY(y)};
	}
	exitImmersiveOnInput();
}

void ExperienceRuntime::drawImmersiveScene() const {
	ofBackground(0);
	const ofTexture& tex = sceneFbo_.getTexture();
	if (!tex.isAllocated()) return;
	const float winW = static_cast<float>(ofGetWidth());
	const float winH = static_cast<float>(ofGetHeight());
	const float scale = std::max(winW / tex.getWidth(), winH / tex.getHeight()); // cover
	const float drawW = tex.getWidth() * scale;
	const float drawH = tex.getHeight() * scale;
	ofPushStyle();
	ofSetColor(255);
	tex.draw((winW - drawW) * 0.5f, (winH - drawH) * 0.5f, drawW, drawH);
	ofPopStyle();
}

void ExperienceRuntime::keyPressed(int key) {
	// IMMERSIVE: any key only exits (and is consumed).
	if (exitImmersiveOnInput()) return;

	// Experience Settings page: F1 / ',' open it; while open it consumes every key.
	if (handleSettingsKey(key)) return;

	// IMMERSIVE: keyboard 'i' or the FULLSCREEN control — distinct from
	// native window fullscreen, which is 'f'.
	if (key == 'i' || key == 'I') {
		enterImmersive();
		return;
	}

	// HUD skins: 'v' cycles every scheduled skin (manual — the schedule
	// pauses), 'V' (Shift+v) resumes the day/night schedule.
	if ((key == 'v' || key == 'V') && skinSchedule_.valid()) {
		if (key == 'v') {
			skinAuto_ = false;
			applySkin(skinSchedule_.nextSkin(currentSkin_), "manual ('v')");
		} else {
			skinAuto_ = true;
			skinPeriod_.reset();
			tickSkinSchedule(true);
		}
		return;
	}

	// Development-only typography playground: consumes only its own keys
	// (all unbound elsewhere); absent in production.
	if (typography_ && typography_->handleKey(key)) return;

	// OC-005: Escape/back cancels an open Orbit editor (restoring the prior configuration) and is consumed;
	// it never reaches the runtime exit binding while the editor is open.
	if (hudCompositorBridge_.orbitEditorOpen()) {
		if (auto d = inputRouter_.translateToDpad(key); d && *d == DpadInput::Back) {
			hudCompositorBridge_.orbitEditorBack();
			orbitEditorClosedAt_ = steadySeconds();
			ofLogNotice("OrbitEditor") << "cancelled (prior configuration restored)";
			return;
		}
	}

	// Interactive region: while active it captures ONLY the arrows + Enter/Esc; every other key
	// (media, scene, mode ...) still reaches its normal route below.
	if (hudCompositorBridge_.interactiveActive() && handleInteractiveKey(key)) return;

	// -- Playable HUD Menu v0.1: focus navigation (arrows / Enter) -------
	if (auto focusInput = inputRouter_.translateToFocusInput(key, ofGetKeyPressed(OF_KEY_SHIFT))) {
		handleFocusInput(*focusInput);
		return;
	}
	if (inputRouter_.isEnergyAdjustToggle(key)) {
		installation::Request r;
		r.kind = installation_.energyAdjusting() ? installation::Request::Kind::EndEnergyAdjust
		                                         : installation::Request::Kind::BeginEnergyAdjust;
		installation_.submit(r);
		ofLogNotice("Installation") << (installation_.energyAdjusting() ? "ENERGY adjustment open" : "ENERGY adjustment closed")
			<< " (target " << ofToString(installation_.settings().energyTarget, 2) << ")";
		return;
	}

	// -- Contract-shaped command routing: two separate paths -----------
	if (auto sceneCommand = inputRouter_.translateToSceneCommand(key)) {
		bool accepted = sceneManager_.dispatchSceneCommand(*sceneCommand);
		ofLogNotice("ExperienceRuntime") << "SceneCommand dispatched, accepted=" << accepted;
		return;
	}
	if (auto runtimeCommand = inputRouter_.translateToRuntimeCommand(key)) {
		handleRuntimeCommand(*runtimeCommand);
		return;
	}

	// Blob First Production Acceptance narrow patch: manual scene-only
	// (pre-HUD-composite) capture, for visual-parity evidence gathering —
	// see saveSceneFrameCaptureForTesting()'s own comment. Deliberately
	// scene-agnostic (works for whichever scene is actually installed),
	// unlike the FakeScene-only dev hooks below.
	if (key == 's' || key == 'S') {
		bool saved = saveSceneFrameCaptureForTesting("captures/scene_only_manual.png");
		ofLogNotice("ExperienceRuntime") << "Scene-only capture "
			<< (saved ? "saved to captures/scene_only_manual.png" : "FAILED (FBO not allocated?)");
		return;
	}

	// -- Development-only test hooks, deliberately NOT part of the
	//    contract command routing above -------------------------------
	FakeScene& scene = sceneManager_.devScene();
	switch (key) {
		case '3':
			// Direct IEcopunkScene::reset() — kept separate from
			// SceneCommand::Reset ('1' above) on purpose.
			scene.reset();
			break;
		case 'c':
		case 'C':
			scene.setContaminateGLStateOnDraw(!scene.contaminateGLStateOnDraw());
			ofLogNotice("ExperienceRuntime") << "FakeScene GL contamination now "
				<< (scene.contaminateGLStateOnDraw() ? "ON" : "off");
			break;
		case '4': {
			// Cycles Ready -> Loading -> Degraded -> Failed -> Ready.
			SceneHealth next;
			switch (scene.testHealth()) {
				case SceneHealth::Ready:    next = SceneHealth::Loading; break;
				case SceneHealth::Loading:  next = SceneHealth::Degraded; break;
				case SceneHealth::Degraded: next = SceneHealth::Failed; break;
				case SceneHealth::Failed:   next = SceneHealth::Ready; break;
				default:                    next = SceneHealth::Ready; break;
			}
			scene.setTestHealth(next);
			break;
		}
		case '5': {
			// Cycles through every SemanticVariant, in declaration order.
			using SV = FakeScene::SemanticVariant;
			SV next;
			switch (scene.semanticVariant()) {
				case SV::Absent:         next = SV::Complete; break;
				case SV::Complete:       next = SV::MissingSignals; break;
				case SV::MissingSignals: next = SV::ZeroSignals; break;
				case SV::ZeroSignals:    next = SV::ZeroMetrics; break;
				case SV::ZeroMetrics:    next = SV::ManyMetrics; break;
				case SV::ManyMetrics:    next = SV::Absent; break;
				default:                 next = SV::Absent; break;
			}
			scene.setSemanticVariant(next);
			break;
		}
		case '6': {
			// Cycles NoSnapshot -> PresentEmpty -> PresentActive -> NoSnapshot.
			using ES = FakeScene::EffectActivityTestState;
			ES next;
			switch (scene.effectActivityTestState()) {
				case ES::NoSnapshot:    next = ES::PresentEmpty; break;
				case ES::PresentEmpty:  next = ES::PresentActive; break;
				case ES::PresentActive: next = ES::NoSnapshot; break;
				default:                next = ES::NoSnapshot; break;
			}
			scene.setEffectActivityTestState(next);
			break;
		}
		default:
			break;
	}
}

void ExperienceRuntime::handleRuntimeCommand(RuntimeCommand command) {
	switch (command) {
		case RuntimeCommand::NextScene:
		case RuntimeCommand::PreviousScene:
			// Scene-switch commands: per the contract's draw-order note,
			// SceneManager "handles any pending RuntimeCommands itself"
			// — read here as specifically the scene-selection subset.
			// RT-003: accepted only while Idle; the outgoing scene's last
			// rendered frame is already retained in sceneFbo_ (this runs
			// between frames, after that frame's draw), and SceneManager
			// deactivates the outgoing scene as part of acceptance.
			sceneManager_.handleSceneSwitchCommand(command);
			break;
		case RuntimeCommand::ToggleHud:
			hudCompositorBridge_.setVisible(!hudCompositorBridge_.isVisible());
			break;
		case RuntimeCommand::ToggleFullscreen: {
			// Through the one installation-control authority (applied at the
			// start of the next frame); the persistent launch preference is
			// separate and unchanged.
			installation::Request r;
			r.kind = installation::Request::Kind::ToggleFullscreen;
			installation_.submit(r);
			break;
		}
		case RuntimeCommand::Exit:
			// Development-only binding (ESC).
			ofExit();
			break;
	}
}
