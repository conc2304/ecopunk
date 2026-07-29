#pragma once

#include <functional>
#include <vector>
#include "TFFragmentTransition.h"
#include "TFPattern.h"
#include "TimeOffsetVideoBuffer.h"
#include "ofVec2f.h"

// Structurally different from every other pattern in this phase: free-
// floating, overlapping, square crops that spawn/age/die rather than a
// persistent grid of cells that reassign playheads in place — closer to a
// Resolume-style particle effect. Does NOT use TFPatternFragment/
// TFShapeFragmentRenderer (that machinery is built around "persistent cell,
// reassign in place", which doesn't apply here) or TFFragmentTransition
// (built around dissolving between two *known* textures, not fading a
// single fragment into/out of existence) — see
// docs/temporal-fields-new-patterns-brief.md Section 6 and Section 0.
//
// Each particle still shares the same playhead-texture pool every other
// pattern draws from (Section 7) — it just never mutates the pool itself;
// see spawnParticle() in the .cpp for why picking (not jumping) a playhead
// is the right policy here.
class TFPatternParticleField : public TFPattern {
	public:
		enum class DriftDirection { OMNIDIRECTIONAL, UPWARD, DOWNWARD };
		enum class DepthOrder { NEWEST_ON_TOP, LARGEST_BEHIND };

		struct Params {
			float spawnRate = 3.0f; // particles/second
			int maxParticleCount = 60; // prototype ran up to ~200; see Section 0's texture-pool cost note
			float minSize = 0.05f; // fraction of the shorter canvas edge
			float maxSize = 0.18f;
			float minLife = 3.0f; // seconds
			float maxLife = 8.0f;
			float driftSpeed = 1.0f;
			DriftDirection driftDirection = DriftDirection::OMNIDIRECTIONAL;

			// Reuses the same weighted style picker and duration every other
			// pattern's *transitions* use — deliberate reuse, not a shared
			// mechanism: see reveal()/drawParticle() in the .cpp for how each
			// style actually fades a single fragment in/out of existence.
			float transitionDuration = 0.8f;
			float hardCutWeight = 33.0f;
			float crossfadeWeight = 34.0f;
			float erosionWeight = 33.0f;

			DepthOrder depthOrder = DepthOrder::NEWEST_ON_TOP;
		};

		void setup(TimeOffsetVideoBuffer* videoBuffer, int canvasW, int canvasH, const Params& params);
		void setParams(const Params& p) { params = p; }

		void reset(int seed) override;
		void update(float dt) override;
		void draw() override;
		void resizeCanvas(int canvasW, int canvasH) override;

		void setOnFragmentReassigned(std::function<void(float nx, float ny)> cb) override { onFragmentReassignedCb = std::move(cb); }
		std::vector<ofVec2f> getActiveFragmentCenters() const override;

	private:
		struct Particle {
			ofVec2f pos;
			ofVec2f velocity;
			float size = 0.0f;
			float age = 0.0f;
			float lifespan = 1.0f;
			int playheadIndex = -1; // fixed at spawn — no mid-life reassignment
			TFFragmentTransition::Style style = TFFragmentTransition::Style::HARD_CUT;
		};

		void spawnParticle();
		void drawParticle(const Particle& p) const;
		float existenceAlpha(const Particle& p) const; // HARD_CUT/CROSSFADE
		float existenceProgress(const Particle& p) const; // EROSION

		TimeOffsetVideoBuffer* videoBuffer = nullptr;
		int canvasW = 0;
		int canvasH = 0;
		Params params{};

		std::vector<Particle> particles;
		float spawnTimer = 0.0f;
		float noiseTime = 0.0f;

		std::function<void(float nx, float ny)> onFragmentReassignedCb;
};
