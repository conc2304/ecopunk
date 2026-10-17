#include "BlobPresetCoverage.h"

#include "BlobDetectionProfileStore.h"
#include "MediaCatalog.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>

namespace fs = std::filesystem;

namespace blobpresetcoverage {

const char * toString(Result r) {
	switch (r) {
		case Result::Pass: return "PASS";
		case Result::Warn: return "WARNING";
		case Result::Error: return "ERROR";
	}
	return "ERROR";
}

bool discoverMedia(const std::string & mediaRoot, std::vector<std::string> & relativePaths, std::string & error) {
	relativePaths.clear();
	std::error_code ec;
	if (!fs::is_directory(mediaRoot, ec)) {
		error = "canonical media root not found: " + mediaRoot;
		return false;
	}
	fs::directory_iterator it(mediaRoot, ec);
	if (ec) {
		error = "cannot enumerate media root " + mediaRoot + ": " + ec.message();
		return false;
	}
	for (const fs::directory_entry & e : it) {
		std::string name = e.path().filename().string();
		std::string ext = e.path().extension().string();
		std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		// ofDirectory::listDir() + allowExt("mp4"): top level only, hidden
		// entries skipped, extension compared case-insensitively.
		if (name.empty() || name[0] == '.' || ext != ".mp4") {
			continue;
		}
		relativePaths.push_back(name);
	}
	std::sort(relativePaths.begin(), relativePaths.end());
	return true;
}

Report evaluate(const std::string & mediaRoot, const std::string & profilesPath, const std::string & catalogPath) {
	Report r;

	// -- Production media set ---------------------------------------------
	std::vector<std::string> discovered;
	std::string error;
	if (!discoverMedia(mediaRoot, discovered, error)) {
		r.errors.push_back(error);
	}
	std::vector<MediaCatalogEntryOverride> overrides;
	if (!catalogPath.empty()) {
		std::vector<std::string> catalogErrors;
		overrides = loadCatalogOverridesFromJsonFile(catalogPath, catalogErrors);
		for (const std::string & e : catalogErrors) {
			r.errors.push_back("media catalog: " + e);
		}
	}
	MediaCatalogBuildResult built = MediaCatalog::build(discovered, overrides);
	for (const std::string & e : built.errors) {
		r.errors.push_back("media catalog: " + e); // e.g. duplicate media ids
	}
	for (const MediaMetadata & m : built.entries) {
		if (m.enabled) { // the selection policy never plays disabled entries
			r.media.push_back({ m.relativePath, m.mediaId, m.fallbackDisplayTitle });
		}
	}

	// -- Presets ----------------------------------------------------------
	BlobDetectionProfileStore store;
	if (!fs::exists(profilesPath)) {
		r.errors.push_back("Blob detection profile file not found: " + profilesPath);
	} else {
		BlobDetectionProfileStore::LoadStatus st = store.load(profilesPath);
		if (st != BlobDetectionProfileStore::LoadStatus::Loaded) {
			for (const std::string & m : store.messages()) {
				r.errors.push_back(m);
			}
			if (store.messages().empty()) {
				r.errors.push_back("Blob detection profile file could not be loaded: " + profilesPath);
			}
		} else {
			// Individually invalid entries are malformed config, not "no preset".
			for (const std::string & m : store.messages()) {
				r.errors.push_back(m);
			}
		}
	}

	if (!r.errors.empty()) {
		r.result = Result::Error;
		return r;
	}

	std::set<std::string> mediaIds;
	for (const MediaItem & m : r.media) {
		mediaIds.insert(m.mediaId);
		(store.find(m.mediaId) != nullptr ? r.configured : r.missing).push_back(m);
	}
	for (const std::string & id : store.mediaIds()) {
		if (mediaIds.count(id) == 0) {
			r.staleProfileIds.push_back(id);
		}
	}
	if (r.media.empty()) {
		r.notes.push_back("no production media found under " + mediaRoot);
	}
	r.result = r.missing.empty() ? Result::Pass : Result::Warn;
	return r;
}

std::string format(const Report & r, const std::string & profilesPath) {
	std::ostringstream ss;
	ss << "Blob detection preset coverage\n"
	   << "------------------------------\n"
	   << "Media files:        " << r.media.size() << "\n"
	   << "Configured:         " << r.configured.size() << "\n"
	   << "Missing:            " << r.missing.size() << "\n"
	   << "Stale presets:      " << r.staleProfileIds.size() << "\n"
	   << "Profiles:           " << profilesPath << "\n";

	if (r.result == Result::Error) {
		ss << "\nERROR: Blob detection preset inputs are invalid:\n";
		for (const std::string & e : r.errors) {
			ss << "  - " << e << "\n";
		}
		ss << "\nFix the inputs above; this cannot be overridden.\n";
	}
	if (r.result == Result::Warn) {
		ss << "\nWARNING: Blob detection presets missing for " << r.missing.size() << " media item"
		   << (r.missing.size() == 1 ? "" : "s") << "\n\n";
		for (const MediaItem & m : r.missing) {
			ss << "  - " << m.title << "\n"
			   << "    " << m.mediaId << "\n"
			   << "    media/" << m.relativePath << "\n";
		}
		ss << "\nRuntime fallback: canonical Blob detector defaults will be used for these videos.\n"
		   << "\nTo configure:\n"
		   << "  1. open blob-region-prototype\n"
		   << "  2. navigate to the missing media (n / p)\n"
		   << "  3. tune detector values\n"
		   << "  4. save the preset (s)\n"
		   << "  5. rerun deploy\n";
	}
	if (!r.staleProfileIds.empty()) {
		ss << "\nNote: " << r.staleProfileIds.size() << " Blob preset" << (r.staleProfileIds.size() == 1 ? " has" : "s have")
		   << " no matching current media item (kept, not deleted):\n";
		for (const std::string & id : r.staleProfileIds) {
			ss << "  - " << id << "\n";
		}
	}
	for (const std::string & n : r.notes) {
		ss << "\nNote: " << n << "\n";
	}
	if (r.result == Result::Pass) {
		ss << "\nBlob detection presets: " << r.configured.size() << "/" << r.media.size() << " configured\n";
	}
	ss << "\nResult: " << toString(r.result) << "\n";
	return ss.str();
}

} // namespace blobpresetcoverage
