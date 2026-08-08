#pragma once

#include <string>
#include <vector>

// Round-trip proof for EffectKnowledgePack, run automatically at debugger
// startup (see ofApp::setup()) rather than requiring a human to press [k]
// and manually inspect the result. Exercises the REAL
// exportEffectKnowledgePack()/importEffectKnowledgePack() functions and the
// REAL EffectKnowledgeBase persistence — nothing here is mocked. Chosen
// over a standalone OF-free test (like shared/src/video-effects/test/)
// because EffectKnowledgePack's I/O genuinely needs ofJson/ofFileUtils;
// chosen over driving the actual GUI via synthetic keystrokes because that
// would pop a real window and requires OS-level input-injection permissions
// this environment cannot reliably grant.
//
// All scratch state lives under bin/data/knowledge_roundtrip_selftest/ and
// is safe to delete; nothing here touches the debugger's real
// bin/data/knowledge/ or bin/data/shared-video-effects/knowledge/ state.
namespace videoeffects {

	// `knownEffectIds` should be the real registered catalog (e.g.
	// service.registry().allIds()) so the unknown-effect-id rejection path
	// is exercised against real data, not a fabricated list.
	//
	// Returns true iff every check passed. Logs each failure via
	// ofLogError("KnowledgePackSelfTest") with enough detail to diagnose
	// without a debugger, and a final PASS/FAIL summary line.
	bool runKnowledgePackRoundTripSelfTest(const std::vector<std::string> & knownEffectIds);

} // namespace videoeffects
