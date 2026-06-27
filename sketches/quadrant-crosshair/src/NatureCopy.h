#pragma once
#include <string>
#include <vector>
#include <algorithm>

// ── Nature Copy System ────────────────────────────────────────────────────────
//
// Classifies a video filename into one of six nature categories and returns a
// full set of HUD copy (DataCard titles, gauge labels, reticle target names).
//
// File naming convention for user-created videos:
//   YYYY-MM-DDTHH-MM-SS_<description>.mp4
//
// Include at least one keyword from the target category in the description.
// Keywords matched by substring on the lowercased full filename. Priority order:
//   FAUNA > FLORAL > IGNIS > HYDRO > TERRA > AETHER > NATURE (fallback)
//
// Keyword reference:
//   HYDRO  — ocean, sea, wave, waves, water, rain, river, lake, tide, surf,
//             aqua, marine, drop, drops, mist, splash, turquoise, stormy, turbulent
//   TERRA  — forest, foliage, fern, tree, trees, leaf, leaves, lush, moss,
//             bark, canopy, woodland, branch, root, grass, meadow
//   FLORAL — flower, bloom, petal, botanical, lilly, lily, rose, blossom,
//             floral, orchid, tulip
//   IGNIS  — fire, flame, bonfire, burn, burning, ember, blaze, ignite, smoke, torch
//   FAUNA  — snail, bird, fish, animal, creature, insect, butterfly, beetle,
//             bee, worm, spider, frog
//   AETHER — sky, aerial, orbit, wind, atmosphere, cloud, clouds, air, breeze,
//             gust, horizon, altitude

enum class NatureCategory { HYDRO, TERRA, FLORAL, IGNIS, FAUNA, AETHER, NATURE };

struct NatureCopy {
    NatureCategory           category;
    std::string              categoryName;
    std::string              cardTitles[4];
    std::string              motionGaugeLabel;
    std::string              dwellGaugeLabel;
    std::vector<std::string> reticleLabels;  // exactly 6
};

inline NatureCategory classifyFilename(const std::string& path) {
    // Extract basename
    size_t slash = path.rfind('/');
    std::string base = (slash == std::string::npos) ? path : path.substr(slash + 1);
    size_t dot = base.rfind('.');
    if (dot != std::string::npos) base = base.substr(0, dot);

    // Lowercase
    std::string s = base;
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);

    auto has = [&](const std::vector<std::string>& kws) -> bool {
        for (const auto& kw : kws)
            if (s.find(kw) != std::string::npos) return true;
        return false;
    };

    // Priority order: most specific first to avoid false positives
    static const std::vector<std::string> kFauna  = {
        "snail","bird","fish","animal","creature","insect",
        "butterfly","beetle","bee","worm","spider","frog"
    };
    static const std::vector<std::string> kFloral = {
        "flower","bloom","petal","botanical","lilly","lily",
        "rose","blossom","floral","orchid","tulip"
    };
    static const std::vector<std::string> kIgnis  = {
        "fire","flame","bonfire","burn","burning",
        "ember","blaze","ignite","smoke","torch"
    };
    static const std::vector<std::string> kHydro  = {
        "water","ocean","rain","wave","waves","sea","river","lake",
        "tide","surf","aqua","marine","drop","drops","mist","splash",
        "turquoise","stormy","turbulent"
    };
    static const std::vector<std::string> kTerra  = {
        "forest","foliage","fern","tree","trees","leaf","leaves",
        "lush","moss","bark","canopy","woodland","branch","root","grass","meadow"
    };
    static const std::vector<std::string> kAether = {
        "sky","aerial","orbit","wind","atmosphere","cloud","clouds",
        "air","breeze","gust","horizon","altitude"
    };

    if (has(kFauna))  return NatureCategory::FAUNA;
    if (has(kFloral)) return NatureCategory::FLORAL;
    if (has(kIgnis))  return NatureCategory::IGNIS;
    if (has(kHydro))  return NatureCategory::HYDRO;
    if (has(kTerra))  return NatureCategory::TERRA;
    if (has(kAether)) return NatureCategory::AETHER;
    return NatureCategory::NATURE;
}

inline NatureCopy getCopyForCategory(NatureCategory cat) {
    NatureCopy c;
    c.category = cat;
    switch (cat) {
        case NatureCategory::HYDRO:
            c.categoryName     = "HYDRO";
            c.cardTitles[0]    = "TIDAL 00";
            c.cardTitles[1]    = "TIDAL 01";
            c.cardTitles[2]    = "TIDAL 02";
            c.cardTitles[3]    = "TIDAL 03";
            c.motionGaugeLabel = "CURRENT";
            c.dwellGaugeLabel  = "DEPTH";
            c.reticleLabels    = {"SURGE","BASIN","DRIFT","SALT","FLOW","DEEP"};
            break;
        case NatureCategory::TERRA:
            c.categoryName     = "TERRA";
            c.cardTitles[0]    = "GROVE 00";
            c.cardTitles[1]    = "GROVE 01";
            c.cardTitles[2]    = "GROVE 02";
            c.cardTitles[3]    = "GROVE 03";
            c.motionGaugeLabel = "MYCEL";
            c.dwellGaugeLabel  = "CANOPY";
            c.reticleLabels    = {"ROOT","SPORE","FERN","BARK","LUSH","MOSS"};
            break;
        case NatureCategory::FLORAL:
            c.categoryName     = "FLORAL";
            c.cardTitles[0]    = "BLOOM 00";
            c.cardTitles[1]    = "BLOOM 01";
            c.cardTitles[2]    = "BLOOM 02";
            c.cardTitles[3]    = "BLOOM 03";
            c.motionGaugeLabel = "POLLEN";
            c.dwellGaugeLabel  = "PETALS";
            c.reticleLabels    = {"STAMEN","PETAL","SPORE","NECTAR","SEPAL","BLOOM"};
            break;
        case NatureCategory::IGNIS:
            c.categoryName     = "IGNIS";
            c.cardTitles[0]    = "EMBER 00";
            c.cardTitles[1]    = "EMBER 01";
            c.cardTitles[2]    = "EMBER 02";
            c.cardTitles[3]    = "EMBER 03";
            c.motionGaugeLabel = "HEAT";
            c.dwellGaugeLabel  = "CHAR";
            c.reticleLabels    = {"EMBER","PLUME","ASH","IGNITE","TORCH","CORE"};
            break;
        case NatureCategory::FAUNA:
            c.categoryName     = "FAUNA";
            c.cardTitles[0]    = "TRACK 00";
            c.cardTitles[1]    = "TRACK 01";
            c.cardTitles[2]    = "TRACK 02";
            c.cardTitles[3]    = "TRACK 03";
            c.motionGaugeLabel = "VITAL";
            c.dwellGaugeLabel  = "DWELL";
            c.reticleLabels    = {"SIGNAL","TRACE","NEST","BURROW","SCAN","FEED"};
            break;
        case NatureCategory::AETHER:
            c.categoryName     = "AETHER";
            c.cardTitles[0]    = "ORBIT 00";
            c.cardTitles[1]    = "ORBIT 01";
            c.cardTitles[2]    = "ORBIT 02";
            c.cardTitles[3]    = "ORBIT 03";
            c.motionGaugeLabel = "DRIFT";
            c.dwellGaugeLabel  = "ALTITUDE";
            c.reticleLabels    = {"CANOPY","WIND","VORTEX","FRONT","APEX","CLOUD"};
            break;
        default: // NATURE fallback
            c.categoryName     = "NATURE";
            c.cardTitles[0]    = "FIELD 00";
            c.cardTitles[1]    = "FIELD 01";
            c.cardTitles[2]    = "FIELD 02";
            c.cardTitles[3]    = "FIELD 03";
            c.motionGaugeLabel = "BIO SIGNAL";
            c.dwellGaugeLabel  = "DWELL";
            c.reticleLabels    = {"CANOPY","FLOW","SPORE","ROOT","SIGNAL","WATER"};
            break;
    }
    return c;
}
