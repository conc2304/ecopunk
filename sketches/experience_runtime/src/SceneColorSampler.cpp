#include "SceneColorSampler.h"

#include "ofGraphics.h"
#include "ofLog.h"
#include "ofAppRunner.h"
#include "ofGLUtils.h"
#include "ofPixels.h"
#include "ofTexture.h"
#include "ofShader.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <chrono>

namespace scenecolorsampling {

namespace {
double nowMs() {
	using namespace std::chrono;
	return duration<double, std::milli>(steady_clock::now().time_since_epoch()).count();
}
int halve(int v) { return std::max(1, (v + 1) / 2); }
}

void SceneColorSampler::releaseFence() {
	if (fence_) {
		glDeleteSync(static_cast<GLsync>(fence_));
		fence_ = nullptr;
	}
}

void SceneColorSampler::releaseLinFence() {
	if (linFence_) {
		glDeleteSync(static_cast<GLsync>(linFence_));
		linFence_ = nullptr;
	}
}

// Fence polling needs sync objects: GL 3.2, or GL_ARB_sync on an older (compatibility) context. Checked at
// runtime - nothing is assumed from the platform. Without them (GLES2, GL 2.1 without the extension) the
// Deferred path keeps its frame-count deferral; EXPERIENCE_RUNTIME_SCENE_COLOR_DEFER=frames[:N] forces it.
void SceneColorSampler::chooseDeferStrategy() {
	defer_ = Defer::Frames;
	deferFrames_ = kDeferFrames;
	std::string want = "auto";
	if (const char* e = std::getenv("EXPERIENCE_RUNTIME_SCENE_COLOR_DEFER")) want = e;
	deferFlush_ = std::getenv("EXPERIENCE_RUNTIME_SCENE_COLOR_DEFER_FLUSH") != nullptr;
	bool syncOk = false;
#ifndef TARGET_OPENGLES
	if (glFenceSync && glClientWaitSync && glDeleteSync) {
		auto r = ofGetGLRenderer();
		const int major = r ? r->getGLVersionMajor() : 0, minor = r ? r->getGLVersionMinor() : 0;
		if (major > 3 || (major == 3 && minor >= 2)) {
			syncOk = true;
		} else if (const GLubyte* ext = glGetString(GL_EXTENSIONS)) {
			syncOk = std::strstr(reinterpret_cast<const char*>(ext), "GL_ARB_sync") != nullptr;
		}
	}
#endif
	if (want.rfind("frames", 0) == 0) {
		const auto colon = want.find(':');
		if (colon != std::string::npos) deferFrames_ = std::min(30, std::max(kDeferFrames, std::atoi(want.c_str() + colon + 1)));
	} else if (syncOk && (want == "auto" || want == "fence")) {
		defer_ = Defer::Fence;
	} else if (want == "fence") {
		ofLogNotice("SceneColor") << "EXPERIENCE_RUNTIME_SCENE_COLOR_DEFER=fence requested but sync objects are unavailable: using frame deferral";
	}
	ofLogNotice("SceneColor") << "deferred readback: syncObjects=" << (syncOk ? "yes" : "no") << " strategy=" << deferStrategy()
		<< " deferFrames=" << deferFrames_ << " flushAfterFence=" << (deferFlush_ ? "yes" : "no");
}

void SceneColorSampler::shutdown() {
	releaseFence();
	releaseLinFence();
	gate_.cancelPending();
}

void SceneColorSampler::recordGap(double t, uint64_t gen) {
	Sample gap;
	gap.t = t;
	gap.generation = gen;
	gap.valid = false;
	if (!series_.push(gap)) stats_.droppedGeneration++;
	stats_.gaps++;
}

void SceneColorSampler::beginGeneration(uint64_t generation, double now) {
	releaseFence();
	releaseLinFence();
	if (gate_.cancelPending()) stats_.droppedGeneration++; // a deferred sample never crosses a scene change
	series_.beginGeneration(generation, now);
}

void SceneColorSampler::ensureChain(int w, int h) {
	if (w == srcW_ && h == srcH_ && chainStages_ > 0) return;
	srcW_ = w;
	srcH_ = h;
	chainStages_ = 0;
	if (const char* e = std::getenv("EXPERIENCE_RUNTIME_SCENE_COLOR_PROBE")) probeMax_ = std::max(8, std::atoi(e));
	int cw = w, ch = h;
	while (chainStages_ < static_cast<int>(chain_.size())) {
		cw = halve(cw);
		ch = halve(ch);
		chain_[chainStages_].allocate(cw, ch, GL_RGBA);
		chain_[chainStages_].getTexture().setTextureMinMagFilter(GL_LINEAR, GL_LINEAR);
		chainStages_++;
		if (cw <= probeMax_ && ch <= probeMax_) break;
	}
	probeW_ = cw;
	probeH_ = ch;
	if (pbo_.isAllocated()) pbo_.allocate(probeW_ * probeH_ * 4, GL_STREAM_READ); // reallocate on a size change
	else pbo_.allocate(probeW_ * probeH_ * 4, GL_STREAM_READ);
}

// Transfer-characteristics validation (Phase "validate FBO format, transfer, alpha, background").
// The measurement assumes the SceneFBO is 8-bit RGBA, display-encoded sRGB (no sRGB internal format,
// no framebuffer-sRGB), cleared to (0,0,0,0) so alpha is meaningful and composited over black.
// Anything else would silently mis-measure, so it becomes explicit gaps plus a one-time log.
bool SceneColorSampler::validateSource(const ofTexture& tex) {
	const int w = static_cast<int>(tex.getWidth()), h = static_cast<int>(tex.getHeight());
	const GLint internalFormat = tex.getTextureData().glInternalFormat;
	if (capabilitiesChecked_ && formatChecked_ && w == validatedW_ && h == validatedH_ && internalFormat == validatedFormat_) return formatOk_;
	if (!capabilitiesChecked_) {
		capabilitiesChecked_ = true;
#ifdef TARGET_OPENGLES
		asyncUsable_ = false; // GLES2: no sync objects / PBO readback
#else
		auto r = ofGetGLRenderer();
		asyncUsable_ = r && (r->getGLVersionMajor() > 3 || (r->getGLVersionMajor() == 3 && r->getGLVersionMinor() >= 2));
#endif
		if (mode_ == ReadbackMode::Async && !asyncUsable_) {
			ofLogNotice("SceneColor") << "async readback needs GL 3.2 sync objects: using synchronous readback";
		}
	}
	validatedW_ = w;
	validatedH_ = h;
	validatedFormat_ = internalFormat;
	formatChecked_ = true;
	bool srgbFramebuffer = false;
#ifndef TARGET_OPENGLES
	srgbFramebuffer = glIsEnabled(GL_FRAMEBUFFER_SRGB) == GL_TRUE;
#endif
	formatOk_ = rawvideo::sourceFormatSupported(sourceKind_, internalFormat, srgbFramebuffer);
	ofLogNotice("SceneColor") << (sourceKind_ == rawvideo::SourceKind::DecodedVideo ? "DecodedVideo " : "SceneFBO ") << w << "x" << h << " internalFormat=0x" << std::hex << internalFormat << std::dec
		<< " framebufferSRGB=" << (srgbFramebuffer ? "on" : "off") << " -> "
		<< (formatOk_ ? "validated: 8-bit display-encoded RGBA, alpha composited over black, sRGB EOTF before averaging"
		              : "UNSUPPORTED for measurement: samples are recorded as gaps");
	return formatOk_;
}


namespace {
// One reduction pass: each output pixel averages a block of source texels. When `linearise` is set every
// source texel is first composited over black (rgb * alpha) and converted with the sRGB EOTF, so the average
// is taken in LINEAR light. Source texels are fetched at exact texel centres (nearest) - never through
// bilinear filtering of encoded values.
std::string reduceFragment(bool programmable, bool rect) {
	std::string f;
	f += programmable ? "#version 150\nout vec4 outputColor;\n" : "#version 120\n";
	f += rect ? "uniform sampler2DRect tex;\n" : "uniform sampler2D tex;\n";
	f += "uniform vec2 srcSize;\nuniform vec2 block;\nuniform float linearise;\n";
	f += "vec3 eotf(vec3 c){ return mix(pow((c + 0.055) / 1.055, vec3(2.4)), c / 12.92, step(c, vec3(0.04045))); }\n";
	f += "void main(){\n";
	f += "  vec2 o = floor(gl_FragCoord.xy);\n";
	f += "  vec2 start = floor(o * block);\n";
	f += "  vec2 stop = min(srcSize, max(start + 1.0, floor((o + 1.0) * block + 0.5)));\n";
	f += "  vec3 acc = vec3(0.0); float n = 0.0;\n";
	f += "  for (int j = 0; j < 24; j++) { float y = start.y + float(j); if (y >= stop.y) break;\n";
	f += "    for (int i = 0; i < 24; i++) { float x = start.x + float(i); if (x >= stop.x) break;\n";
	f += rect ? "      vec4 c = " + std::string(programmable ? "texture" : "texture2DRect") + "(tex, vec2(x + 0.5, y + 0.5));\n"
	          : "      vec4 c = " + std::string(programmable ? "texture" : "texture2D") + "(tex, vec2((x + 0.5) / srcSize.x, (y + 0.5) / srcSize.y));\n";
	f += "      vec3 e = c.rgb * c.a; if (linearise > 0.5) e = eotf(e);\n";
	f += "      acc += e; n += 1.0; } }\n";
	f += std::string("  vec3 m = acc / max(n, 1.0);\n") + (programmable ? "  outputColor = vec4(m, 1.0);\n" : "  gl_FragColor = vec4(m, 1.0);\n") + "}\n";
	return f;
}
std::string reduceVertex(bool programmable) {
	return programmable
		? "#version 150\nuniform mat4 modelViewProjectionMatrix;\nin vec4 position;\nvoid main(){ gl_Position = modelViewProjectionMatrix * position; }\n"
		: "#version 120\nvoid main(){ gl_Position = ftransform(); }\n";
}
} // namespace

bool SceneColorSampler::setupLinearPath() {
	if (linearTried_) return linearOk_;
	linearTried_ = true;
	linearOk_ = false;
	if (std::getenv("EXPERIENCE_RUNTIME_SCENE_COLOR_FALLBACK")) { // test switch: exercise the capability fallback
		ofLogNotice("SceneColor") << "EXPERIENCE_RUNTIME_SCENE_COLOR_FALLBACK set: using the encoded halving chain + CPU linearisation";
		return false;
	}
#ifdef TARGET_OPENGLES
	ofLogNotice("SceneColor") << "GLES: GPU linear-light reduction unavailable (no guaranteed half-float render targets); using the encoded halving chain + CPU linearisation (lower accuracy on high-contrast detail)";
	return false;
#else
	const bool programmable = ofIsGLProgrammableRenderer();
	for (int rect = 0; rect < 2; ++rect) {
		ofShader& sh = rect ? reduceRect_ : reduce2D_;
		const bool ok = sh.setupShaderFromSource(GL_VERTEX_SHADER, reduceVertex(programmable))
			&& sh.setupShaderFromSource(GL_FRAGMENT_SHADER, reduceFragment(programmable, rect != 0))
			&& (!programmable || sh.bindDefaults()) && sh.linkProgram();
		if (!ok) {
			ofLogNotice("SceneColor") << "reduction shader failed to build: using the encoded fallback path";
			return false;
		}
	}
	ofFbo::Settings st;
	st.internalformat = GL_RGBA16F;
	st.textureTarget = GL_TEXTURE_2D;
	st.minFilter = GL_NEAREST;
	st.maxFilter = GL_NEAREST;
	st.useDepth = false;
	st.useStencil = false;
	st.numSamples = 0;
	st.width = 16;  st.height = 9;
	for (auto& f : linBs_) f.allocate(st);
	st.width = 160; st.height = 90;
	linA_.allocate(st);
	linearOk_ = linA_.isAllocated() && linBs_[0].isAllocated() && linBs_[1].isAllocated();
	ofLogNotice("SceneColor") << (linearOk_ ? "GPU linear-light reduction ready (RGBA16F: 160x90 -> 16x9)"
		: "half-float render targets unavailable: using the encoded fallback path");
	if (linearOk_) chooseDeferStrategy();
	return linearOk_;
#endif
}

void SceneColorSampler::submitLinear(const ofTexture& scene, int slot) {
	const float sw = scene.getWidth(), sh = scene.getHeight();
	const bool rect = scene.getTextureData().textureTarget == GL_TEXTURE_RECTANGLE;
	ofPushStyle();
	ofDisableAlphaBlending();
	ofSetColor(255);
	auto pass = [&](ofFbo& dst, const ofTexture& src, float srcW, float srcH, bool srcRect, bool linearise) {
		ofShader& sh2 = srcRect ? reduceRect_ : reduce2D_;
		dst.begin();
		ofClear(0, 0, 0, 255);
		sh2.begin();
		sh2.setUniformTexture("tex", src, 0);
		sh2.setUniform2f("srcSize", srcW, srcH);
		sh2.setUniform2f("block", srcW / dst.getWidth(), srcH / dst.getHeight());
		sh2.setUniform1f("linearise", linearise ? 1.0f : 0.0f);
		ofDrawRectangle(0, 0, dst.getWidth(), dst.getHeight());
		sh2.end();
		dst.end();
	};
	pass(linA_, scene, sw, sh, rect, true);                      // composite over black + EOTF + box
	pass(linBs_[slot], linA_.getTexture(), linA_.getWidth(), linA_.getHeight(), false, false); // already linear: plain box
	ofPopStyle();
}

bool SceneColorSampler::collectLinear(int slot, double t, uint64_t gen) {
	const double w0 = nowMs();
	linBs_[slot].readToPixels(pxBuf_); // GPU completion wait (Sync) / near-free (Deferred) + 16x9 transfer
	const double w1 = nowMs();
	stats_.wait.add(w1 - w0);
	const std::size_t n = static_cast<std::size_t>(pxBuf_.getWidth()) * pxBuf_.getHeight();
	Sample s;
	s.t = t;
	s.generation = gen;
	if (n > 0) {
		double r = 0, g = 0, b = 0;
		const float* d = pxBuf_.getData();
		const int ch = static_cast<int>(pxBuf_.getNumChannels());
		for (std::size_t i = 0; i < n; ++i) { r += d[i * ch]; g += d[i * ch + 1]; b += d[i * ch + 2]; }
		s.r = static_cast<float>(r / n); s.g = static_cast<float>(g / n); s.b = static_cast<float>(b / n);
		s.luma = luma709(s.r, s.g, s.b);
		s.valid = std::isfinite(s.r) && std::isfinite(s.g) && std::isfinite(s.b) && std::isfinite(s.luma); // never publish NaN/inf as a colour
	}
	const double c1 = nowMs();
	stats_.convert.add(c1 - w1);
	if (!s.valid) stats_.gaps++;
	if (!series_.push(s)) stats_.droppedGeneration++;
	stats_.completed++;
	stats_.push.add(nowMs() - c1);
	return s.valid;
}

bool SceneColorSampler::sampleLinearGpu(const ofTexture& scene, double now) {
	const double t0 = nowMs();
	const int slot = nextSlot_;
	nextSlot_ = (nextSlot_ + 1) % 2;
	submitLinear(scene, slot);
	if (mode_ == ReadbackMode::Deferred && defer_ == Defer::Fence) {
		releaseLinFence();
		linFence_ = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
		if (deferFlush_) glFlush();
	}
	stats_.submit.add(nowMs() - t0);
	if (mode_ == ReadbackMode::Deferred) { // read it once the GPU has finished it
		gate_.issued(slot, t0, now, series_.generation(), defer_ == Defer::Fence ? kFenceTimeoutMs : 0.0);
		pendingFrames_ = 0;
		return true;
	}
	const bool ok = collectLinear(slot, now, series_.generation());
	stats_.total.add(nowMs() - t0);
	return ok;
}

void SceneColorSampler::finishSample(const uint8_t* px, double t, uint64_t gen) {
	Sample s;
	s.t = t;
	s.generation = gen;
	s.valid = averageLinearRgba8OverBlack(px, static_cast<std::size_t>(probeW_) * static_cast<std::size_t>(probeH_), s.r, s.g, s.b);
	if (s.valid) {
		s.luma = luma709(s.r, s.g, s.b);
	} else {
		stats_.gaps++;
	}
	if (!series_.push(s)) stats_.droppedGeneration++;
	stats_.completed++;
}

void SceneColorSampler::onLiveFrame(const ofTexture& sceneTexture, double now) {
	if (!series_.due(now)) return;
	if (fence_ || gate_.pending()) { // an async / deferred readback is still in flight
		stats_.droppedBusy++;
		return;
	}
	if (!sceneTexture.isAllocated()) return;
	if (!validateSource(sceneTexture)) { // explicit gaps, never a guessed measurement
		series_.markIssued(now);
		Sample gap;
		gap.t = now;
		gap.generation = series_.generation();
		series_.push(gap);
		stats_.gaps++;
		stats_.unsupportedFormat++;
		return;
	}
	series_.markIssued(now);

	const double t0 = nowMs();
	if (setupLinearPath()) { // exact linear-light GPU reduction, tiny synchronous float readback
		if (mode_ == ReadbackMode::Deferred && !gate_.tryIssue(t0)) { // telemetry disabled by repeated failures: skip, the series goes Stale
			stats_.telemetryDisabledSkips++;
			return;
		}
		sampleLinearGpu(sceneTexture, now);
		stats_.issued++;
		const double dt = nowMs() - t0;
		stats_.issueMsTotal += dt;
		stats_.issueMsMax = std::max(stats_.issueMsMax, dt);
		static const bool selfTestGpu = std::getenv("EXPERIENCE_RUNTIME_SCENE_COLOR_SELFTEST") != nullptr;
		if (selfTestGpu && series_.size() > 0) {
			ofPixels full;
			sceneTexture.readToPixels(full);
			float er, eg, eb;
			if (averageLinearRgba8OverBlack(full.getData(), full.getWidth() * full.getHeight(), er, eg, eb)) {
				const Sample& m = series_.at(series_.size() - 1);
				ofLogNotice("SceneColorSelfTest") << "gen=" << series_.generation() << " probe(rgb,Y)=" << m.r << "," << m.g << "," << m.b << "," << m.luma
					<< " exact(rgb,Y)=" << er << "," << eg << "," << eb << "," << luma709(er, eg, eb)
					<< " dY=" << (m.luma - luma709(er, eg, eb));
			}
		}
		return;
	}
	ensureChain(static_cast<int>(sceneTexture.getWidth()), static_cast<int>(sceneTexture.getHeight()));

	ofPushStyle();
	const ofTexture* src = &sceneTexture;
	for (int i = 0; i < chainStages_; ++i) {
		ofFbo& dst = chain_[i];
		dst.begin();
		ofClear(0, 0, 0, 255); // opaque black: stage 0 composites rgb*a over it
		if (i == 0) ofEnableAlphaBlending(); else ofDisableAlphaBlending();
		ofSetColor(255);
		src->draw(0, 0, dst.getWidth(), dst.getHeight());
		dst.end();
		src = &dst.getTexture();
	}
	ofPopStyle();

	if (effectiveMode() == ReadbackMode::Sync) {
		ofPixels pixels;
		chain_[chainStages_ - 1].getTexture().readToPixels(pixels);
		finishSample(pixels.getData(), now, series_.generation());
		// Accuracy self-test (EXPERIENCE_RUNTIME_SCENE_COLOR_SELFTEST=1, measurement only, sync mode): also read the
		// FULL-resolution frame and log the exact linear-light average beside the probe's value.
		static const bool selfTest = std::getenv("EXPERIENCE_RUNTIME_SCENE_COLOR_SELFTEST") != nullptr;
		if (selfTest) {
			ofPixels full;
			sceneTexture.readToPixels(full);
			float er, eg, eb;
			if (averageLinearRgba8OverBlack(full.getData(), full.getWidth() * full.getHeight(), er, eg, eb) && series_.size() > 0) {
				const Sample& m = series_.at(series_.size() - 1);
				ofLogNotice("SceneColorSelfTest") << "gen=" << series_.generation() << " probe(rgb,Y)=" << m.r << "," << m.g << "," << m.b << "," << m.luma
					<< " exact(rgb,Y)=" << er << "," << eg << "," << eb << "," << luma709(er, eg, eb)
					<< " dY=" << (m.luma - luma709(er, eg, eb));
			}
		}
		stats_.issued++;
		const double dt = nowMs() - t0;
		stats_.issueMsTotal += dt;
		stats_.issueMsMax = std::max(stats_.issueMsMax, dt);
		return;
	}

	chain_[chainStages_ - 1].getTexture().copyTo(pbo_);
	fence_ = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
	glFlush();
	inflightT_ = now;
	inflightGen_ = series_.generation();
	stats_.issued++;
	const double dt = nowMs() - t0;
	stats_.issueMsTotal += dt;
	stats_.issueMsMax = std::max(stats_.issueMsMax, dt);
}

void SceneColorSampler::onStaticFrame(double now) {
	if (!series_.due(now)) return;
	series_.markIssued(now);
	Sample gap;
	gap.t = now;
	gap.generation = series_.generation();
	gap.valid = false;
	series_.push(gap);
	stats_.gaps++;
}

void SceneColorSampler::poll(double) {
	if (gate_.pending()) { // deferred linear readback: never blocks, never falls back to a blocking read
		const double nowT = nowMs();
		FenceStatus status;
		if (defer_ == Defer::Fence) {
			if (!linFence_) {
				status = FenceStatus::WaitFailed; // glFenceSync returned no sync object: this request can never be proven finished
			} else {
				const GLenum r = glClientWaitSync(static_cast<GLsync>(linFence_), 0, 0); // timeout 0: a query, not a wait
				if (r == GL_ALREADY_SIGNALED || r == GL_CONDITION_SATISFIED) status = FenceStatus::Signaled;
				else if (r == GL_TIMEOUT_EXPIRED) status = FenceStatus::NotReady;
				else status = FenceStatus::WaitFailed; // GL_WAIT_FAILED or anything unexpected
			}
		} else { // legacy frame-count deferral (no sync objects / explicit override): no completion check
			status = (++pendingFrames_ >= deferFrames_) ? FenceStatus::Signaled : FenceStatus::NotReady;
		}
		// Diagnostic fault injection (A7 validation only; unset in production): EXPERIENCE_RUNTIME_SCENE_COLOR_FAULT=waitfailed|stuck
		// forces every deferred fence to report GL_WAIT_FAILED / never-signalled so the discard + disable path runs against real GL.
		static const std::string fault = [] { const char* e = std::getenv("EXPERIENCE_RUNTIME_SCENE_COLOR_FAULT"); return std::string(e ? e : ""); }();
		if (!fault.empty() && defer_ == Defer::Fence) {
			if (fault == "waitfailed") status = FenceStatus::WaitFailed;
			else if (fault == "stuck") status = FenceStatus::NotReady;
		}
		const int slot = gate_.slot();
		const double sampleT = gate_.sampleT(), issueMs = gate_.issueMs();
		const uint64_t gen = gate_.generation();
		const bool wasDisabled = gate_.disabled();
		switch (gate_.poll(status, nowT, series_.generation())) {
		case PollAction::None:
			if (status == FenceStatus::NotReady) stats_.notReadyPolls++;
			return;
		case PollAction::Collect: {
			if (defer_ == Defer::Fence) stats_.latency.add(nowT - issueMs);
			releaseLinFence();
			const double t0 = nowMs();
			const bool valid = collectLinear(slot, sampleT, gen);
			gate_.reportResult(valid, nowMs());
			if (!valid) stats_.invalidSamples++;
			stats_.total.add(nowMs() - t0);
			break;
		}
		case PollAction::DiscardWaitFailed:
			releaseLinFence();
			stats_.discardedWaitFailed++;
			recordGap(sampleT, gen);
			ofLogWarning("SceneColor") << "deferred readback discarded: fence wait failed (gen=" << gen << ")";
			break;
		case PollAction::DiscardTimeout:
			releaseLinFence();
			stats_.discardedTimeout++;
			recordGap(sampleT, gen);
			ofLogWarning("SceneColor") << "deferred readback discarded: fence not signalled after " << kFenceTimeoutMs << " ms (gen=" << gen << ")";
			break;
		case PollAction::DiscardGeneration:
			releaseLinFence();
			stats_.droppedGeneration++;
			return;
		}
		if (!wasDisabled && gate_.disabled())
			ofLogWarning("SceneColor") << "scene-colour telemetry DISABLED after " << DeferredReadbackGate::kFailureThreshold
				<< " consecutive deferred-readback failures (scene rendering unaffected; re-probing in " << DeferredReadbackGate::kRecoveryCooldownMs / 1000.0 << " s)";
		else if (wasDisabled && !gate_.disabled())
			ofLogNotice("SceneColor") << "scene-colour telemetry RE-ENABLED after a successful probe";
		return;
	}
	if (effectiveMode() != ReadbackMode::Async || !fence_) return;
	const GLenum r = glClientWaitSync(static_cast<GLsync>(fence_), 0, 0); // never blocks
	if (r != GL_ALREADY_SIGNALED && r != GL_CONDITION_SATISFIED) return;
	const double t0 = nowMs();
	releaseFence();
	if (inflightGen_ != series_.generation()) {
		stats_.droppedGeneration++;
		return;
	}
	if (auto* px = pbo_.map<uint8_t>(GL_READ_ONLY)) {
		finishSample(px, inflightT_, inflightGen_);
		pbo_.unmap();
	} else {
		Sample gap;
		gap.t = inflightT_;
		gap.generation = inflightGen_;
		series_.push(gap);
		stats_.gaps++;
	}
	const double dt = nowMs() - t0;
	stats_.collectMsTotal += dt;
	stats_.collectMsMax = std::max(stats_.collectMsMax, dt);
}

} // namespace scenecolorsampling
