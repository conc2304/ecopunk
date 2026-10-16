// RT-006: compiles shared/src/hud_overlay/organisms/FaultCascadeOrganism.cpp (Fragment Trail's in-artwork
// overlay) into ExperienceRuntime — a wrapper translation unit so the file's
// relative includes (e.g. "../HudOverlayOrganism.h") resolve from its real
// directory. HudOverlayDialPanel (ofxGui) is deliberately not compiled here.
#include "../../../shared/src/hud_overlay/organisms/FaultCascadeOrganism.cpp"
