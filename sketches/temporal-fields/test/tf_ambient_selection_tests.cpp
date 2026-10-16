// TEMP-008B: headless tests for tfambient::pickForCycle() (TFAmbientSelection.h)
// — the per-composition-cycle underlay/overlay pick. No oF linkage.
// Build/run: make -C test -f Makefile.tests test
#include "../src/TFAmbientSelection.h"

#include <cstdio>
#include <set>
#include <vector>

static int failures = 0;
static void check(bool ok, const char* what) {
	std::printf("%s: %s\n", ok ? "PASS" : "FAIL", what);
	if (!ok) failures++;
}

int main() {
	using tfambient::pickForCycle;
	using tfambient::kOverlaySalt;
	using tfambient::kUnderlaySalt;

	check(pickForCycle(0, 1234, kUnderlaySalt, -1) == -1, "empty group selects nothing (active count 0)");

	bool singleOk = true;
	for (int seed = 1; seed < 2000; seed++) singleOk &= pickForCycle(1, seed, kUnderlaySalt, 0) == 0;
	check(singleOk, "single-entry group always selects that entry (even as the previous pick)");

	bool rangeOk = true, repeatOk = true, deterministicOk = true;
	for (int count : {2, 3, 9, 10}) {
		int previous = -1;
		for (int seed = 1; seed < 5000; seed++) {
			const int pick = pickForCycle(count, seed, kUnderlaySalt, previous);
			rangeOk &= pick >= 0 && pick < count;
			repeatOk &= pick != previous;
			deterministicOk &= pick == pickForCycle(count, seed, kUnderlaySalt, previous);
			previous = pick;
		}
	}
	check(rangeOk, "picks are valid indices into the group");
	check(repeatOk, "no immediate repeat across consecutive cycles when the group has >1 entry");
	check(deterministicOk, "same seed + previous pick -> same selection (deterministic)");

	std::set<int> seenU, seenO;
	int samePairs = 0;
	const int cycles = 2000;
	for (int seed = 1; seed <= cycles; seed++) {
		const int u = pickForCycle(10, seed, kUnderlaySalt, -1);
		const int o = pickForCycle(10, seed, kOverlaySalt, -1);
		seenU.insert(u);
		seenO.insert(o);
		samePairs += u == o;
	}
	check(seenU.size() == 10 && seenO.size() == 10, "every entry of each group is reachable");
	// Independent picks of two 10-entry groups coincide ~10% of the time; a
	// shared (non-independent) pick would coincide 100%.
	check(samePairs > cycles / 20 && samePairs < cycles / 5, "underlay and overlay picks are independent");

	std::vector<int> histogram(9, 0);
	for (int seed = 1; seed <= 9000; seed++) histogram[pickForCycle(9, seed, kOverlaySalt, -1)]++;
	bool spreadOk = true;
	for (int h : histogram) spreadOk &= h > 700 && h < 1300; // 1000 expected each
	check(spreadOk, "picks spread roughly evenly over a 9-entry group");

	std::printf("%s (%d failure%s)\n", failures == 0 ? "ALL PASSED" : "FAILED", failures, failures == 1 ? "" : "s");
	return failures == 0 ? 0 : 1;
}
