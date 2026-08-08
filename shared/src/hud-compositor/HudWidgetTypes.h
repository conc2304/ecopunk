#pragma once

// ============================================================================
// HudWidgetTypes.h — the renderer-private HudWidgetType enum.
//
// This is deliberately NOT HUD-Semantic-Slot-Model-v1.md §15's
// HudWidgetType (Text/StatusBadge/NumericValue/ProgressBar/ProgressRing/
// Sparkline/Gauge/EffectList/ChannelStrip/Timeline/AmbientField) — that
// enum belongs to a draft, unapproved shared document. This enum instead
// matches exactly the approved candidate widget set this task and
// docs/probes/hud-runtime-validation-studio-probe.md both name: Label,
// Value, Status Badge, Progress Bar, Progress Ring, Effect Chips, Metadata
// Card, Icon Strip, Timeline — plus Sparkline (needed for the "activity
// trace"/"secondary trace" universal regions this task's region catalog
// explicitly requires, and separately justified per the Semantic Slot
// Model draft §16's history mechanism, not "because an existing widget
// supports it" per this task's own instruction about Histogram/Radar/
// Sparkline/Mini Graph), ChannelStrip (needed for Quadrant's four-channel
// telemetry, per the probe's §11.5 evidence and the addendum's §12
// "four quadrant states" candidate), AmbientField (needed for the empty-
// flexible-region ambient fallback this task requires), and the
// tooling-only BindingPlaceholder (Validation-Studio-only, per this
// task's §7).
//
// Icon Strip is named in the approved candidate set but is not
// implemented as a distinct widget type in this increment: it requires an
// icon-asset atlas, and "no icon-asset or icon-strip rendering exists
// anywhere in the repo" per the probe (§5's gap table) — the art pipeline
// that would produce icon assets is explicitly out of scope until after
// the wireframe is accepted (frozen constraint: "Production skin artwork
// is deferred until the wireframe is accepted"). Deferred, not silently
// dropped — see this increment's report, "Deviations from prompt".
// ============================================================================

#include <cstdint>

namespace hudpresent {

enum class HudWidgetType : uint8_t {
	Label,
	StatusBadge,
	NumericValue,
	ProgressBar,
	ProgressRing,
	Sparkline,
	EffectChips,
	MetadataCard,
	ChannelStrip,
	AmbientField,
	Timeline,           // binding contract fully defined; rendering deferred (see TimelineWidget.h)
	BindingPlaceholder  // tooling-only — Validation Studio, never production
};

} // namespace hudpresent
