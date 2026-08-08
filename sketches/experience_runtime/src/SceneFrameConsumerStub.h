#pragma once

// SceneFrameConsumerStub has been superseded by HudCompositorStub (see
// HudCompositorStub.h) as of Development Stream 1 — the boundary consumer
// now receives the fully-assembled HudFrameData (SceneFrame + SceneHudStatus
// + SceneManagerStatus + SceneCapabilities + RuntimeTelemetry), not a bare
// SceneFrame, per that task's approved direction ("the compositor cannot
// poll scenes or services directly").
//
// This file is intentionally left empty (rather than deleted) because file
// deletion required elevated permissions not available in this session —
// see the Development Stream 1 implementation report, "Deviations from
// prompt". It defines nothing and is not included anywhere.
