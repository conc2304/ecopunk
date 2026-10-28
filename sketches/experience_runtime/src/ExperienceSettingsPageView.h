#pragma once

#include "ExperienceSettingsPage.h"

// Draws the Experience Settings page in the canonical 1280x720 composition
// canvas (same space as the HUD): plain boxes and bitmap text, with focus,
// editing and disabled states carried by text markers and outlines — never by
// colour alone. Pure presentation; owns no state.
void drawExperienceSettingsPage(const settingspage::Page& page);
