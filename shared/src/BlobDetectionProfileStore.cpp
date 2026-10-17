#include "BlobDetectionProfileStore.h"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <sstream>

using nlohmann::json;

BlobDetectionProfile resolveProfile(const BlobDetectionProfile & defaults, const BlobDetectionProfile * over) {
	BlobDetectionProfile out = defaults;
	if (over == nullptr) {
		return out;
	}
	if (over->threshold) out.threshold = over->threshold;
	if (over->minBlobAreaNormalized) out.minBlobAreaNormalized = over->minBlobAreaNormalized;
	if (over->maxBlobAreaNormalized) out.maxBlobAreaNormalized = over->maxBlobAreaNormalized;
	if (over->erodeIterations) out.erodeIterations = over->erodeIterations;
	if (over->dilateIterations) out.dilateIterations = over->dilateIterations;
	return out;
}

std::string describeProfile(const std::string & mediaId, const std::string & source, const BlobDetectionProfile & p) {
	auto num = [](const auto & v) {
		std::ostringstream s;
		if (v) {
			s << *v;
		} else {
			s << "-";
		}
		return s.str();
	};
	std::ostringstream ss;
	ss << "[BLOB DETECTION PROFILE]\n"
	   << "media=" << mediaId << "\n"
	   << "source=" << source << "\n"
	   << "threshold=" << num(p.threshold) << "\n"
	   << "minArea=" << num(p.minBlobAreaNormalized) << "\n"
	   << "maxArea=" << num(p.maxBlobAreaNormalized) << "\n"
	   << "erode=" << num(p.erodeIterations) << "\n"
	   << "dilate=" << num(p.dilateIterations);
	return ss.str();
}

void BlobDetectionProfileStore::reset() {
	document_ = json::object();
	document_["schemaVersion"] = kSchemaVersion;
	document_["profiles"] = json::object();
	profiles_.clear();
	messages_.clear();
	writable_ = true;
}

BlobDetectionProfileStore::LoadStatus BlobDetectionProfileStore::load(const std::string & path) {
	std::ifstream in(path);
	if (!in) {
		reset();
		messages_.push_back("no profile file at " + path + " (all media use detector defaults)");
		return LoadStatus::Missing;
	}
	std::stringstream buffer;
	buffer << in.rdbuf();
	return loadFromString(buffer.str());
}

bool BlobDetectionProfileStore::parseEntry(const json & entry, BlobDetectionProfile & out, std::string & why) {
	if (!entry.is_object()) {
		why = "entry is not an object";
		return false;
	}
	auto readInt = [&](const char * key, int lo, int hi, std::optional<int> & dst) {
		if (!entry.contains(key)) return true;
		const json & v = entry[key];
		if (!v.is_number_integer() || v.get<long long>() < lo || v.get<long long>() > hi) {
			why = std::string(key) + " must be an integer in [" + std::to_string(lo) + "," + std::to_string(hi) + "]";
			return false;
		}
		dst = v.get<int>();
		return true;
	};
	auto readUnit = [&](const char * key, std::optional<double> & dst) {
		if (!entry.contains(key)) return true;
		const json & v = entry[key];
		if (!v.is_number() || v.get<double>() < 0.0 || v.get<double>() > 1.0) {
			why = std::string(key) + " must be a number in [0,1]";
			return false;
		}
		dst = v.get<double>();
		return true;
	};
	BlobDetectionProfile p;
	if (!readInt("threshold", 0, 255, p.threshold) || !readUnit("minBlobAreaNormalized", p.minBlobAreaNormalized)
		|| !readUnit("maxBlobAreaNormalized", p.maxBlobAreaNormalized) || !readInt("erodeIterations", 0, 8, p.erodeIterations)
		|| !readInt("dilateIterations", 0, 8, p.dilateIterations)) {
		return false;
	}
	if (p.minBlobAreaNormalized && p.maxBlobAreaNormalized && *p.minBlobAreaNormalized >= *p.maxBlobAreaNormalized) {
		why = "minBlobAreaNormalized must be < maxBlobAreaNormalized";
		return false;
	}
	out = p;
	return true;
}

BlobDetectionProfileStore::LoadStatus BlobDetectionProfileStore::loadFromString(const std::string & text) {
	reset();
	json doc = json::parse(text, nullptr, false);
	auto invalid = [&](const std::string & why) {
		reset();
		writable_ = false;
		messages_.push_back("profile file rejected: " + why + " (all media use detector defaults; saving disabled)");
		return LoadStatus::Invalid;
	};
	if (doc.is_discarded() || !doc.is_object()) {
		return invalid("not a JSON object");
	}
	if (!doc.contains("schemaVersion") || !doc["schemaVersion"].is_number_integer()
		|| doc["schemaVersion"].get<int>() != kSchemaVersion) {
		return invalid("schemaVersion must be " + std::to_string(kSchemaVersion));
	}
	if (!doc.contains("profiles") || !doc["profiles"].is_object()) {
		return invalid("missing \"profiles\" object");
	}
	document_ = doc;
	for (auto it = doc["profiles"].begin(); it != doc["profiles"].end(); ++it) {
		BlobDetectionProfile p;
		std::string why;
		if (it.key().empty()) {
			messages_.push_back("profile with empty media id skipped");
		} else if (parseEntry(it.value(), p, why)) {
			profiles_.emplace_back(it.key(), p);
		} else {
			messages_.push_back("profile '" + it.key() + "' skipped (media uses defaults): " + why);
		}
	}
	return LoadStatus::Loaded;
}

std::vector<std::string> BlobDetectionProfileStore::mediaIds() const {
	std::vector<std::string> ids;
	for (const auto & kv : profiles_) {
		ids.push_back(kv.first);
	}
	std::sort(ids.begin(), ids.end());
	return ids;
}

const BlobDetectionProfile * BlobDetectionProfileStore::find(const std::string & mediaId) const {
	for (const auto & kv : profiles_) {
		if (kv.first == mediaId) {
			return &kv.second;
		}
	}
	return nullptr;
}

bool BlobDetectionProfileStore::set(const std::string & mediaId, const BlobDetectionProfile & profile, std::string & error) {
	if (mediaId.empty()) {
		error = "no canonical media id — not saving";
		return false;
	}
	std::string why;
	json entry = document_["profiles"].contains(mediaId) && document_["profiles"][mediaId].is_object()
		? document_["profiles"][mediaId]
		: json::object();
	auto put = [&](const char * key, const auto & v) {
		if (v) {
			entry[key] = *v;
		} else {
			entry.erase(key);
		}
	};
	put("threshold", profile.threshold);
	put("minBlobAreaNormalized", profile.minBlobAreaNormalized);
	put("maxBlobAreaNormalized", profile.maxBlobAreaNormalized);
	put("erodeIterations", profile.erodeIterations);
	put("dilateIterations", profile.dilateIterations);
	BlobDetectionProfile check;
	if (!parseEntry(entry, check, why)) {
		error = "invalid profile: " + why;
		return false;
	}
	document_["profiles"][mediaId] = entry;
	for (auto & kv : profiles_) {
		if (kv.first == mediaId) {
			kv.second = check;
			return true;
		}
	}
	profiles_.emplace_back(mediaId, check);
	return true;
}

std::string BlobDetectionProfileStore::serialize() const {
	// nlohmann::json objects are std::map-backed: keys come out sorted.
	return document_.dump(2) + "\n";
}

bool BlobDetectionProfileStore::save(const std::string & path, std::string & error) const {
	if (!writable_) {
		error = "the profile file on disk failed to load; fix or move it before saving (refusing to overwrite)";
		return false;
	}
	std::string tmp = path + ".tmp";
	{
		std::ofstream out(tmp, std::ios::trunc);
		if (!out) {
			error = "cannot write " + tmp;
			return false;
		}
		out << serialize();
		if (!out) {
			error = "write failed for " + tmp;
			return false;
		}
	}
	if (std::rename(tmp.c_str(), path.c_str()) != 0) {
		error = "cannot replace " + path;
		std::remove(tmp.c_str());
		return false;
	}
	return true;
}

std::optional<BlobDetectionProfileTracker::Applied> BlobDetectionProfileTracker::observe(
	const std::optional<std::string> & mediaId, const BlobDetectionProfileStore & store, const BlobDetectionProfile & defaults) {
	if (!mediaId || mediaId->empty() || (lastMediaId_ && *lastMediaId_ == *mediaId)) {
		return std::nullopt;
	}
	lastMediaId_ = mediaId;
	const BlobDetectionProfile * hit = store.find(*mediaId);
	Applied a { *mediaId, hit != nullptr ? "per_media" : "defaults", resolveProfile(defaults, hit) };
	last_ = a;
	return a;
}
