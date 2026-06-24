#pragma once

#include "CompositionBase.h"
#include "BEFragment.h"
#include "VideoSampler.h"

// Blueprint Emergence's placement algorithm (§06): geometry/size selection,
// random candidate sampling, scoring, and zone bookkeeping.
class BEComposition : public CompositionBase {
	public:
		void setupBE(GridSystem* grid, VideoSampler* videoSampler, int canvasW, int canvasH, int dividerCol);

		bool isZoneALight() const { return zoneALight; }

	protected:
		bool attemptPlacement() override;
		void onCycleStart() override;

	private:
		struct Candidate {
			int col, row;
			float score;
			Fragment* nearest;
		};

		GeometryType pickGeometryType() const;
		void pickSize(GeometryType type, int& w, int& h) const;
		float overlapFraction(int col, int row, int w, int h) const;
		int countZoneFragments(bool zoneA) const;
		ofColor pickPlaceholderColor() const;
		ofRectangle boundsForCell(int col, int row, int w, int h) const;
		void requestVideoTexture(BEFragment* fragment, int w, int h) const;
		void placeCircleFragment(); // once-per-cycle event; bypasses scored-candidate flow

		VideoSampler* videoSampler   = nullptr;
		int canvasW                  = 0;
		int canvasH                  = 0;
		int dividerCol               = 0;
		bool zoneALight              = false;

		// Circle placement state — reset each cycle in onCycleStart()
		int  circleTriggerCount      = 0;
		int  placementCount          = 0;
		bool circleSpawnedThisCycle  = false;
};
