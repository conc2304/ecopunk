#!/usr/bin/env bash
# Fails (non-zero exit) if more than one implementation of any canonical HUD
# widget .cpp file exists anywhere in the repository. The canonical, single
# compiled copy of this library lives at shared/src/hud/ — see
# shared/src/hud/README.md. This exists because the library was previously
# copy-pasted into two sketches independently and drifted; this check is
# meant to make that mistake loud instead of silent.
#
# Run from anywhere inside the repo (it locates the repo root itself).
# Usage: scripts/check-hud-library-uniqueness.sh

set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$REPO_ROOT" || exit 1

# Canonical widget + shared-contract source files. Add a name here whenever a
# new widget or shared HUD header/source is added to shared/src/hud/.
HUD_FILES="
ScannerWidget.cpp
ScannerWidget.h
ContourWidget.cpp
ContourWidget.h
HexGridWidget.cpp
HexGridWidget.h
GaugeWidget.cpp
GaugeWidget.h
DataCardWidget.cpp
DataCardWidget.h
FlowFieldWidget.cpp
FlowFieldWidget.h
NodeNetworkWidget.cpp
NodeNetworkWidget.h
ReticleWidget.cpp
ReticleWidget.h
TickBurstWidget.cpp
TickBurstWidget.h
RadarStationWidget.cpp
RadarStationWidget.h
BreathingTickClusterWidget.cpp
BreathingTickClusterWidget.h
TelemetryReadoutWidget.cpp
TelemetryReadoutWidget.h
HalftonePatchWidget.cpp
HalftonePatchWidget.h
TextCalloutWidget.cpp
TextCalloutWidget.h
DashedLineWidget.cpp
DashedLineWidget.h
HudWidget.h
HudFrameRenderer.cpp
HudFrameRenderer.h
HudTypes.h
HudUtils.h
HudElements.h
"

status=0

for name in $HUD_FILES; do
	# Exclude build output (obj/, bin/) and version control internals — only
	# real source-tree copies count as duplicates.
	matches="$(find . \( -path '*/obj/*' -o -path '*/bin/*' -o -path '*/.git/*' \) -prune -o -type f -name "$name" -print | sort)"
	count="$(printf '%s\n' "$matches" | grep -c . || true)"

	if [ "$count" -gt 1 ]; then
		echo "DUPLICATE: $name found in $count locations:" >&2
		printf '%s\n' "$matches" | sed 's/^/  /' >&2
		status=1
	elif [ "$count" -eq 0 ]; then
		echo "MISSING: $name not found anywhere in the repo (expected exactly one copy under shared/src/hud/)." >&2
		status=1
	fi
done

if [ "$status" -eq 0 ]; then
	echo "OK: exactly one copy of every canonical HUD source file found."
fi

exit "$status"
