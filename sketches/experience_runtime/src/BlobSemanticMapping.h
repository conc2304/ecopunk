#pragma once

#include "SceneSemanticTypes.h"

#include <cstdint>

// BlobSemanticMapping.h — Blob First Production Acceptance narrow patch.
//
// Pure, openFrameworks-free extraction of the derivation logic that used to
// live entirely inside BlobProductionScene::buildSemanticData() (Blob First
// Complete Production Migration). Behavior-preserving: every bucketing
// threshold, clamp, and metric definition here is copied unchanged from that
// method — this file exists ONLY to make that logic testable with a bare
// compiler (see sketches/experience_runtime/test/blob_semantic_mapping_tests.cpp),
// the same "OF-free .h/.cpp pair" pattern already established by
// shared/src/video-effects/knowledge/EffectActivityStatus.h and
// shared/src/video-playback/VideoPlaybackStatus.h. SceneSemanticTypes.h
// itself has zero OF dependency (confirmed: only <cstdint>/<optional>/
// <string>/<vector>), so this header/cpp pair has none either.
//
// This is scene-local, domain-owned code — NOT a shared/frozen contract.
// It does not appear in shared/src/scene/, does not change
// SceneSemanticTypes.h, and is not referenced by any scene other than
// BlobProductionScene (sketches/experience_runtime/src/BlobProductionScene.cpp),
// which now just builds an Inputs value from its own real state and forwards
// to compute() below.
namespace blobsemantics {

// Real, already-computed Blob pipeline state — every field here has an
// honest, cheap source in BlobSceneCore (see BlobProductionScene::
// buildSemanticData()'s call site). No field is fabricated or decorative.
struct Inputs {
	int regionCount = 0;           // BlobSceneCore::trackCount()
	int fragmentCount = 0;         // BlobSceneCore::activeFragmentCount()
	int maxActiveFragments = 1;    // BlobSceneCore::regionParams().maxActiveFragments
	float occupiedAreaFraction = 0.0f; // BlobSceneCore::occupiedAreaFraction()
	float activeSeconds = 0.0f;    // BlobProductionScene's own real elapsed-active timer
	uint64_t generation = 0;       // BlobProductionScene's own real activation counter
};

// Only ever called once the caller has already decided a semantic snapshot
// should be reported (real video frame, scene set up and active) — see
// BlobProductionScene::hudStatus()'s own absent/present decision, which is
// NOT part of this function (that decision needs OF-dependent state this
// pure function deliberately does not have access to).
SceneSemanticData compute(const Inputs & in);

} // namespace blobsemantics
