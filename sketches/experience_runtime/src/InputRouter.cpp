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
