#pragma once

// ============================================================================
// HudDataTypes.h — the typed value model resolved sources are expressed in.
//
// Renderer-private, HUD Runtime and Validation Studio domain. Deliberately
// has ZERO openFrameworks dependency (no ofMain.h, no ofColor/ofRectangle,
// no GL types) so that every piece of logic built on top of it — the
// resolver, profile compiler, formatting/vocabulary services, missing-data
// controller, history store, and fake scenarios — can be exercised by the
// repo's existing standalone-test convention (see
// shared/src/hud-compositor-test/, modeled on
// sketches/temporal-fields/test/tf_timeline_tests.cpp: a bare `c++
// -std=c++17` compile, no OF linkage, no window/GL context). Only the
// widget *drawing* layer (shared/src/hud-compositor/widgets/) and the
// interactive Validation Studio app need OF, and both consume
// HudResolvedValue by value from this header without needing to draw
// anything to build/test the logic above them.
//
// This is NOT a shared/approved contract. It exists entirely inside the HUD
// Runtime and Validation Studio domain's own renderer-private boundary, per
// this task's "Implementation strategy" instruction. Nothing here is
// referenced by shared/src/scene/SceneContract.h, and nothing here should
// be treated as a proposal to change it.
// ============================================================================

#include "SceneSemanticTypes.h" // real, approved, OF-free (shared/src/scene/) — see the alias below

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace hudpresent {

// Engineering Session 2 reconciliation: HUD-Semantic-Slot-Model-v1.md §7's
// HudDataClass proposal is no longer a draft — it landed, verbatim, as a
// real, approved, global-namespace ::HudDataClass in
// shared/src/scene/SceneSemanticTypes.h (confirmed OF-free: that header
// pulls in nothing but <cstdint>/<optional>/<string>/<vector>). Session 1
// mirrored this enum as a renderer-private copy because the real type
// didn't exist yet; keeping a second, drifting copy now that it does would
// be exactly the "duplicate of shared type" this session's reconciliation
// pass is scoped to eliminate. This alias — not a fresh redeclaration —
// means every existing `hudpresent::HudDataClass::Literal`-shaped call
// site in this domain keeps compiling unchanged while actually referring
// to the one real, shared enum.
using HudDataClass = ::HudDataClass;

// The renderer-facing value shapes a compiled binding's resolved source can
// take. This is a superset built for widget consumption, not a 1:1 copy of
// the draft model's HudMetricValueType — Boolean/Text/IdentifierList are
// added here because control-availability, free-form messages, and
// effects.active respectively need them and the draft model does not yet
// cover those slot families.
enum class HudSourceValueType : uint8_t {
	Scalar,
	Count,
	Ratio,
	DurationSeconds,
	Boolean,
	Identifier,
	Text,
	IdentifierList
};

// Fixed-capacity identifier list — avoids std::vector's heap growth on the
// per-resolve hot path. kMaxItems matches the HUD Semantic Slot Model v1
// draft §20's "maximum active effect IDs: 8" recommendation, reused here
// for every identifier-list-shaped source (effects.active today; any
// future list-shaped source should reuse the same cap rather than invent a
// new one). `overflowed` lets a caller/test distinguish "exactly 8 items"
// from "the source actually had more than 8 and got truncated" without
// silently losing that fact.
struct HudIdentifierList {
	static constexpr size_t kMaxItems = 8;

	std::array<std::string, kMaxItems> items{};
	size_t count = 0;
	bool overflowed = false;

	bool empty() const { return count == 0; }

	// Returns false (and leaves the list unchanged) once kMaxItems is
	// reached; sets `overflowed` so callers/tests can tell truncation
	// happened instead of silently under-reporting a source's real size.
	bool push(const std::string& id) {
		if (count >= kMaxItems) {
			overflowed = true;
			return false;
		}
		items[count++] = id;
		return true;
	}
};

// One resolved source value, as handed from HudSourceResolver to a
// compiled binding and, after HudMissingDataController's policy is
// applied, to a widget's draw call.
//
// Deliberately a flat struct with one field per possible payload kind
// rather than std::variant: every field except `textValue` is a plain
// scalar (free to leave default-initialized when unused), and
// `textValue`/`listValue`'s std::string members rely on small-string
// optimization for the short IDs/labels this domain actually produces
// (scene/vocabulary/effect IDs are consistently well under the ~15-22
// byte SSO threshold on both libstdc++ and libc++) — a real, documented
// trade-off, not a zero-allocation guarantee: an unusually long free-form
// message (`scene.message`/`manager.message`) can still allocate. A
// hand-rolled fixed-char-buffer type would close that gap but was judged
// more complexity than this task's wireframe scope warrants; flagged here
// so a later pass can revisit if profiling ever shows it matters.
struct HudResolvedValue {
	HudSourceValueType valueType = HudSourceValueType::Scalar;
	HudDataClass dataClass = HudDataClass::Literal;

	// false = the source is known (a real slot/metric exists) but its
	// value is not currently available — e.g. an unpopulated
	// std::optional in the underlying fake/adapter data. This is the
	// "missing vs. zero" distinction: present=false is never conflated
	// with numberValue==0.0f / textValue=="" / listValue.empty()==true,
	// each of which is a fully legitimate present=true value.
	bool present = false;

	float numberValue = 0.0f;      // Scalar / Count / Ratio / DurationSeconds
	bool boolValue = false;        // Boolean
	std::string textValue;         // Identifier (vocabulary-resolvable ID) / Text (already-final text)
	HudIdentifierList listValue;   // IdentifierList

	static HudResolvedValue missing(HudSourceValueType type, HudDataClass cls) {
		HudResolvedValue v;
		v.valueType = type;
		v.dataClass = cls;
		v.present = false;
		return v;
	}

	static HudResolvedValue number(HudSourceValueType type, float value, HudDataClass cls = HudDataClass::Literal) {
		HudResolvedValue v;
		v.valueType = type;
		v.dataClass = cls;
		v.present = true;
		v.numberValue = value;
		return v;
	}

	static HudResolvedValue boolean(bool value, HudDataClass cls = HudDataClass::Literal) {
		HudResolvedValue v;
		v.valueType = HudSourceValueType::Boolean;
		v.dataClass = cls;
		v.present = true;
		v.boolValue = value;
		return v;
	}

	static HudResolvedValue identifier(std::string id, HudDataClass cls = HudDataClass::Literal) {
		HudResolvedValue v;
		v.valueType = HudSourceValueType::Identifier;
		v.dataClass = cls;
		v.present = true;
		v.textValue = std::move(id);
		return v;
	}

	static HudResolvedValue text(std::string t, HudDataClass cls = HudDataClass::Literal) {
		HudResolvedValue v;
		v.valueType = HudSourceValueType::Text;
		v.dataClass = cls;
		v.present = true;
		v.textValue = std::move(t);
		return v;
	}

	static HudResolvedValue list(HudIdentifierList l, HudDataClass cls = HudDataClass::Literal) {
		HudResolvedValue v;
		v.valueType = HudSourceValueType::IdentifierList;
		v.dataClass = cls;
		v.present = true;
		v.listValue = std::move(l);
		return v;
	}

	// A present IdentifierList with zero items — legitimately distinct
	// from `present == false` (see HudMissingDataController's "missing
	// and empty lists remain distinct" rule).
	bool isEmptyList() const {
		return valueType == HudSourceValueType::IdentifierList && present && listValue.empty();
	}
};

} // namespace hudpresent
