#include "InputRouter.h"

#include "ofEvents.h" // OF_KEY_ESC and friends

std::optional<SceneCommand> InputRouter::translateToSceneCommand(int key) const {
	switch (key) {
		case '1': return SceneCommand::Reset;
		case '2': return SceneCommand::Regenerate; // FakeScene rejects this — exercises the reject path
		// Blob First Complete Production Migration: the only scene that
		// currently advertises these (BlobProductionScene::capabilities())
		// — 'n'/'p' deliberately chosen, not '3'/'4', since those are
		// already used by ExperienceRuntime::keyPressed()'s FakeScene-only
		// dev hooks (direct reset()/health cycling) reached via the
		// default: fallthrough below this switch.
		case 'n':
		case 'N': return SceneCommand::NextMedia;
		case 'p':
		case 'P': return SceneCommand::PreviousMedia;
		// Scene & Pattern Navigation: device-neutral NEXT MODE (a physical
		// button maps here the same way); the active scene decides.
		case 'm':
		case 'M': return SceneCommand::NextMode;
		default:  return std::nullopt;
	}
}

std::optional<HudFocusInput> InputRouter::translateToFocusInput(int key, bool shift) const {
	switch (key) {
		// OC-005: Tab / Shift+Tab step focus (and the Orbit editor cursor) like the arrows.
		case '\t':          return HudFocusInput{HudFocusInput::Kind::Step, shift ? -1 : +1};
		case OF_KEY_LEFT:
		case OF_KEY_UP:     return HudFocusInput{HudFocusInput::Kind::Step, -1};
		case OF_KEY_RIGHT:
		case OF_KEY_DOWN:   return HudFocusInput{HudFocusInput::Kind::Step, +1};
		case OF_KEY_RETURN: return HudFocusInput{HudFocusInput::Kind::Activate, 0};
		default:            return std::nullopt;
	}
}

std::optional<RuntimeCommand> InputRouter::translateToRuntimeCommand(int key) const {
	switch (key) {
		case 'f':
		case 'F':          return RuntimeCommand::ToggleFullscreen;
		case 'h':
		case 'H':          return RuntimeCommand::ToggleHud;
		case OF_KEY_ESC:   return RuntimeCommand::Exit;
		case '[':          return RuntimeCommand::PreviousScene;
		case ']':          return RuntimeCommand::NextScene;
		default:           return std::nullopt;
	}
}

std::optional<DpadInput> InputRouter::translateToDpad(int key) const {
	switch (key) {
		case OF_KEY_UP:     return DpadInput::Up;
		case OF_KEY_DOWN:   return DpadInput::Down;
		case OF_KEY_LEFT:   return DpadInput::Left;
		case OF_KEY_RIGHT:  return DpadInput::Right;
		case OF_KEY_RETURN: return DpadInput::Ok;
		case OF_KEY_ESC:    return DpadInput::Back;
		default:            return std::nullopt;
	}
}

bool InputRouter::isSettingsToggle(int key) const {
	return key == OF_KEY_F1 || key == ',';
}
