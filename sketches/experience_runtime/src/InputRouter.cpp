#include "InputRouter.h"

#include "ofEvents.h" // OF_KEY_ESC and friends

std::optional<SceneCommand> InputRouter::translateToSceneCommand(int key) const {
	switch (key) {
		case '1': return SceneCommand::Reset;
		case '2': return SceneCommand::Regenerate; // FakeScene rejects this — exercises the reject path
		default:  return std::nullopt;
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
