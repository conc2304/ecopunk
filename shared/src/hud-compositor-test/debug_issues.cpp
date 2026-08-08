#include <iostream>
#include "../hud-compositor/HudPresentationProfile.h"
#include "../hud-compositor/HudProfileCompiler.h"
#include "../hud-compositor/HudRegionCatalog.h"
#include "../hud-compositor/HudVocabularyResolver.h"

using namespace hudpresent;

int main() {
	HudRegionCatalog regions;
	HudVocabularyResolver vocab;
	HudPresentationProfileRegistry registry;
	HudProfileCompiler compiler(regions, vocab);

	auto compiled = compiler.compile("totally-unrecognized-scene-id", registry);
	for (const auto& issue : compiled.issues) {
		std::cout << "binding=" << issue.bindingId << " kind=" << (int)issue.kind
				  << " sev=" << (int)issue.severity << " detail=" << issue.detail << "\n";
	}
	return 0;
}
