#pragma once

class ShaderLibrary;

// Proves TFEffectPicker::activityStatus() -- the real, existing production
// Shared Effects accessor for temporal-fields (Shared Effects "Final
// Canonical Activity Producer Seam Patch") -- satisfies the DEC-015
// accessor contract: side-effect-free, const, stable across repeated
// reads, honest present-empty/health/phase/progress/prominence semantics.
//
// Run automatically at ofApp::setup() (not gated behind a keypress), same
// rationale as sketches/shader-effect-debugger/src/KnowledgePackSelfTest.h:
// provable from a console log without GUI interaction, and exercises the
// REAL compiled TFEffectPicker class, not a mock or a standalone
// reimplementation.
//
// Uses its own, separate TFEffectPicker instance (sharing the real,
// already-loaded ShaderLibrary the real scene also uses) rather than the
// scene's own live picker -- this lets the test force deterministic
// weighted-pick outcomes (all-Raw, or all-one-known-good-effect, or
// all-one-deliberately-unregistered-name) without perturbing the actual
// running scene's own effect selection.
namespace videoeffects {
	bool runActivityStatusSelfTest(ShaderLibrary & sharedShaderLib);
}
