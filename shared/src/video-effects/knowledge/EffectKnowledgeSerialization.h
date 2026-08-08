#pragma once

#include "EffectKnowledgeBase.h"
#include "ofJson.h"

// Internal helper: the single JSON <-> KnowledgeEntry mapping, shared by
// EffectKnowledgeBase.cpp (per-sketch whitelist/blacklist files) and
// EffectKnowledgePack.cpp (cross-scene export/import bundles) so the two
// never drift into two different serializations of the same struct. Not
// included by EffectKnowledgeBase.h itself or anything in the
// EffectActivityStatus.h include chain — this header pulls in ofJson.h and
// is meant for .cpp-only consumption.
namespace videoeffects {

	ofJson knowledgeEntryToJson(const KnowledgeEntry & entry);

	// Returns false (and logs a warning) if this individual entry is too
	// malformed to use, rather than throwing — one bad entry must not take
	// down a whole file/pack.
	bool knowledgeEntryFromJson(const ofJson & j, KnowledgeEntry & out);

} // namespace videoeffects
