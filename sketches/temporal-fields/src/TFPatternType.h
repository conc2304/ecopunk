#pragma once

#include <string>

enum class TFPatternType { BSP, BLOB_GRID, BANDS, COLUMN_GRID, TELESCOPING_FRAMES, PARTICLE_FIELD, ECOLOGICAL_SUCCESSION, NETWORK_GROWTH, TEMPORAL_TIDES };

// Human-readable name (logging/HUD) and the lowercase key each pattern's
// preset JSON is stored/looked-up under (matches the "bsp"/"blobgrid" keys
// TFParameterPanel's preset format already used before this phase).
inline std::string tfPatternTypeName(TFPatternType type) {
	switch (type) {
		case TFPatternType::BSP: return "BSP";
		case TFPatternType::BLOB_GRID: return "BLOB_GRID";
		case TFPatternType::BANDS: return "BANDS";
		case TFPatternType::COLUMN_GRID: return "COLUMN_GRID";
		case TFPatternType::TELESCOPING_FRAMES: return "TELESCOPING_FRAMES";
		case TFPatternType::PARTICLE_FIELD: return "PARTICLE_FIELD";
		case TFPatternType::ECOLOGICAL_SUCCESSION: return "ECOLOGICAL_SUCCESSION";
		case TFPatternType::NETWORK_GROWTH: return "NETWORK_GROWTH";
		case TFPatternType::TEMPORAL_TIDES: return "TEMPORAL_TIDES";
	}
	return "UNKNOWN";
}

inline std::string tfPatternTypePresetKey(TFPatternType type) {
	switch (type) {
		case TFPatternType::BSP: return "bsp";
		case TFPatternType::BLOB_GRID: return "blobgrid";
		case TFPatternType::BANDS: return "bands";
		case TFPatternType::COLUMN_GRID: return "columngrid";
		case TFPatternType::TELESCOPING_FRAMES: return "telescopingframes";
		case TFPatternType::PARTICLE_FIELD: return "particlefield";
		case TFPatternType::ECOLOGICAL_SUCCESSION: return "ecologicalsuccession";
		case TFPatternType::NETWORK_GROWTH: return "networkgrowth";
		case TFPatternType::TEMPORAL_TIDES: return "temporaltides";
	}
	return "unknown";
}

inline TFPatternType tfPatternTypeFromPresetKey(const std::string& key) {
	if (key == "blobgrid") return TFPatternType::BLOB_GRID;
	if (key == "bands") return TFPatternType::BANDS;
	if (key == "columngrid") return TFPatternType::COLUMN_GRID;
	if (key == "telescopingframes") return TFPatternType::TELESCOPING_FRAMES;
	if (key == "particlefield") return TFPatternType::PARTICLE_FIELD;
	if (key == "ecologicalsuccession") return TFPatternType::ECOLOGICAL_SUCCESSION;
	if (key == "networkgrowth") return TFPatternType::NETWORK_GROWTH;
	if (key == "temporaltides") return TFPatternType::TEMPORAL_TIDES;
	return TFPatternType::BSP;
}
