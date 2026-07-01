#pragma once

// Snapshot of composition state, built fresh each frame and handed to
// TriggerBus condition checks. BEComposition fills in everything it owns;
// ofApp fills in maxMeasurementLineConnections, since that bookkeeping
// lives in AnnotationRenderer, not BEComposition.
struct BECompositionState {
	int fragmentCount = 0;
	int zoneACount = 0;
	int zoneBCount = 0;
	float secondsSinceLastPlacement = 0.0f;
	int maxMeasurementLineConnections = 0;
	bool circleHasBeenPlaced = false;
	int currentPhase = 0; // CompositionBase::CyclePhase, cast to int
	float cycleElapsed = 0.0f;
};
