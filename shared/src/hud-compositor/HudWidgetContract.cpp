#include "HudWidgetContract.h"

#include <stdexcept>
#include <unordered_map>

namespace hudpresent {

namespace {

using T = HudSourceValueType;

const std::unordered_map<HudWidgetType, HudWidgetContract>& contractTable() {
	static const std::unordered_map<HudWidgetType, HudWidgetContract> table = [] {
		std::unordered_map<HudWidgetType, HudWidgetContract> m;

		m[HudWidgetType::Label] = {
			HudWidgetType::Label,
			// "text" is deliberately NOT required here (see
			// HudWidgetContract.h's header comment) — a Label binding is
			// valid with either a bound "text" source (dynamic content,
			// e.g. scene_title) or just labelVocabularyId (a static
			// caption, e.g. the controls' nonfunctional labels), or both.
			// HudProfileCompiler enforces "at least one of the two" as a
			// small Label-specific rule instead of a blanket required=true
			// here, which would wrongly reject the static-caption case.
			{ {"text", false, {T::Identifier, T::Text}},
			  {"enabled", false, {T::Boolean}} },
			false
		};

		m[HudWidgetType::StatusBadge] = {
			HudWidgetType::StatusBadge,
			{ {"value", true, {T::Identifier}},
			  {"progress", false, {T::Ratio}} },
			false
		};

		m[HudWidgetType::NumericValue] = {
			HudWidgetType::NumericValue,
			{ {"value", true, {T::Scalar, T::Count, T::Ratio, T::DurationSeconds}},
			  {"label", false, {T::Identifier, T::Text}} },
			false
		};

		m[HudWidgetType::ProgressBar] = {
			HudWidgetType::ProgressBar,
			{ {"value", true, {T::Ratio}},
			  {"label", false, {T::Identifier, T::Text}} },
			false
		};

		m[HudWidgetType::ProgressRing] = {
			HudWidgetType::ProgressRing,
			{ {"value", true, {T::Ratio}},
			  {"label", false, {T::Identifier, T::Text}} },
			false
		};

		m[HudWidgetType::Sparkline] = {
			HudWidgetType::Sparkline,
			{ {"value", true, {T::Scalar, T::Count, T::Ratio}} },
			true
		};

		m[HudWidgetType::EffectChips] = {
			HudWidgetType::EffectChips,
			{ {"value", true, {T::IdentifierList}} },
			false
		};

		m[HudWidgetType::MetadataCard] = {
			HudWidgetType::MetadataCard,
			{ {"title", true, {T::Identifier, T::Text}},
			  {"value", false, {}},   // polymorphic — any resolved value type
			  {"meta", false, {T::Identifier, T::Text}},
			  {"secondary", false, {}} },
			false
		};

		m[HudWidgetType::ChannelStrip] = {
			HudWidgetType::ChannelStrip,
			// Up to HudSlotBinding::kMaxSources (4) channels — only
			// channel0 is required so a 1-4 channel binding both
			// validate; a binding wanting all four (Quadrant's real
			// case) supplies channel0..channel3.
			{ {"channel0", true, {T::Identifier, T::Text}},
			  {"channel1", false, {T::Identifier, T::Text}},
			  {"channel2", false, {T::Identifier, T::Text}},
			  {"channel3", false, {T::Identifier, T::Text}} },
			false
		};

		m[HudWidgetType::AmbientField] = {
			HudWidgetType::AmbientField,
			// Deliberately has NO required role: AmbientField is itself
			// the fallback for when nothing else is available (this
			// task's §9 "empty flexible region may instantiate one
			// ambient fallback"), so it must be constructible with zero
			// bound sources. `influence` is optional and, when present,
			// is what makes the ambient motion "deterministically
			// influenced by real semantic state" per the (draft) Ambient
			// Data Rules (Semantic Slot Model v1 §17) — honored on a
			// best-effort basis in this increment; see this task's
			// report, "Deviations from prompt", for why full enforcement
			// of that rule is not implemented as a compiler check here.
			{ {"influence", false, {T::Scalar, T::Count, T::Ratio}} },
			false
		};

		m[HudWidgetType::Timeline] = {
			HudWidgetType::Timeline,
			{ {"phase", true, {T::Identifier}},
			  {"progress", false, {T::Ratio}} },
			false
		};

		m[HudWidgetType::BindingPlaceholder] = {
			HudWidgetType::BindingPlaceholder,
			{}, // tooling-only; no required sources, accepts anything or nothing
			false
		};

		return m;
	}();
	return table;
}

} // namespace

const HudWidgetContract& HudWidgetContracts::forType(HudWidgetType type) {
	auto it = contractTable().find(type);
	if (it == contractTable().end()) {
		throw std::logic_error("HudWidgetContracts::forType: unregistered HudWidgetType");
	}
	return it->second;
}

} // namespace hudpresent
