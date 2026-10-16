#pragma once

#include <optional>
#include <string>
#include <vector>

// BlobEffectPolicy.h — Blob production fragment-treatment selection policy.
//
// Pure, openFrameworks-free (same convention as BlobSemanticMapping.h), so
// the selection rule is testable with a bare compiler
// (test/blob_effect_policy_tests.cpp).
//
// Visual model (Blob production): the full frame is drawn desaturated, and
// tracked regions are windows that reveal either the source's original
// color or one curated shader treatment. One treatment per canonical Shared
// Video media item, advanced in a fixed order whenever the active mediaId
// changes.
//
// Scene-local, domain-owned code — NOT a shared/frozen contract, not a
// Shared Effects authority (no EffectActivityStatus is produced; Blob's
// HudFrameData.effects stays std::nullopt). Shader treatments render with
// their canonical catalog-default uniforms (DEC-007: no raw-parameter
// randomization).
namespace blobeffects {

// What a Blob fragment shows. Original is a first-class treatment, not a
// shader: it maps to an empty VideoRegionController effectName, which
// VideoRegionEffectRenderer draws as the unshaded source crop.
struct FragmentTreatment {
	enum class Kind { Original, Shader };
	Kind kind = Kind::Original;
	std::string effectId; // canonical effect id; empty for Original

	static FragmentTreatment original() { return {}; }
	static FragmentTreatment shader(const std::string & id) { return { Kind::Shader, id }; }

	bool isOriginal() const { return kind == Kind::Original; }
	// VideoRegionController::Params::effectName for this treatment.
	const std::string & regionEffectName() const { return effectId; }
	// Stable id for logs/tests: "original" or the effect id.
	std::string id() const { return isOriginal() ? std::string("original") : effectId; }

	bool operator==(const FragmentTreatment & o) const { return kind == o.kind && effectId == o.effectId; }
	bool operator!=(const FragmentTreatment & o) const { return !(*this == o); }
};

// The curated production set, in selection order. The first media item gets
// index 0 (Original).
//
// Shader eligibility (see docs/reviews/blob-production-effect-policy-report.md):
// one of BlobSceneCore::effectChoices() that passes the unchanged BLOB-002
// direct-vs-3-stage tolerance in BlobRenderPathAbHarness, so fragments don't
// pop when they switch paths at the end of a fade. desaturate is eligible
// but omitted here: on the desaturated background it would be invisible.
const std::vector<FragmentTreatment> & curatedFragmentTreatments();

// The full-frame background effect for this visual model.
const std::string & backgroundEffectId();

class MediaEffectPolicy {
public:
	// Call once per active update with the canonical
	// VideoPlaybackStatus::mediaId. Returns true only when the selected
	// treatment changed on this call:
	//   - nullopt (no active media): no change.
	//   - first media ever observed: keeps index 0, returns false.
	//   - same mediaId as last time: no change.
	//   - different mediaId: advances one step through the curated list.
	// Never called while Blob is inactive, so media that came and went
	// while another scene was showing counts as one change on return.
	bool observeMedia(const std::optional<std::string> & mediaId);

	const FragmentTreatment & currentTreatment() const;
	int selectionChangeCount() const { return changeCount_; }

private:
	std::optional<std::string> lastMediaId_;
	size_t index_ = 0;
	int changeCount_ = 0;
};

} // namespace blobeffects
