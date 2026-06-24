#!/usr/bin/env bash
# Convert video files to the format required by EcopunkVideoCollage / blueprint_emergence.
#
# Target spec (from docs/blueprint_emergence_engineering_plan.md §05):
#   Container : MP4
#   Codec     : H.264, Baseline or Main profile
#   Resolution: 540p preferred (720p hard max)
#   Frame rate: 24 or 30 fps
#   Audio     : stripped
#   Color     : sRGB / bt709, no HDR / Log
#   Pixel fmt : yuv420p
#
# Usage:
#   ./tools/convert_media.sh <file> [file ...]
#
# For each file the script:
#   1. Probes the file and decides the minimum ffmpeg operation needed.
#   2. Writes the result to sketches/blueprint_emergence/bin/data/media/.
#   3. Deletes the original (unless it was already in the media folder).

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
MEDIA_DIR="$REPO_ROOT/sketches/blueprint_emergence/bin/data/media"

RED='\033[0;31m'
YLW='\033[0;33m'
GRN='\033[0;32m'
RST='\033[0m'

err()  { echo -e "${RED}error:${RST} $*" >&2; }
warn() { echo -e "${YLW}warn:${RST}  $*"; }
info() { echo -e "${GRN}info:${RST}  $*"; }

# ---------- helpers -----------------------------------------------------------

probe() {
    local file="$1" field="$2" stream_sel="${3:--select_streams v:0}"
    ffprobe -v error $stream_sel \
        -show_entries "stream=$field" \
        -of default=noprint_wrappers=1:nokey=1 \
        "$file" 2>/dev/null | head -1
}

probe_format() {
    local file="$1"
    ffprobe -v error \
        -show_entries format=format_name \
        -of default=noprint_wrappers=1:nokey=1 \
        "$file" 2>/dev/null
}

audio_stream_count() {
    ffprobe -v error -select_streams a \
        -show_entries stream=index \
        -of default=noprint_wrappers=1:nokey=1 \
        "$1" 2>/dev/null | wc -l | tr -d ' '
}

fps_to_float() {
    # Converts "30000/1001" or "30" to a float via awk
    echo "$1" | awk -F'/' '{ if (NF==2) printf "%.4f", $1/$2; else printf "%.4f", $1 }'
}

# Returns 0 (true) if fps is close enough to 24 or 30 (±0.5 fps tolerance)
fps_is_acceptable() {
    local fps_str="$1"
    echo "$fps_str" | awk -F'/' '{
        fps = (NF==2) ? $1/$2 : $1
        if ((fps >= 23.5 && fps <= 24.5) || (fps >= 29.5 && fps <= 30.5)) exit 0
        else exit 1
    }'
}

is_hdr() {
    local ct
    ct=$(probe "$1" color_transfer)
    [[ "$ct" == "smpte2084" || "$ct" == "arib-std-b67" ]]
}

# Generates a unique output path, avoiding name collisions
output_path() {
    local input="$1"
    local base
    base="$(basename "${input%.*}")"
    local out="$MEDIA_DIR/${base}.mp4"
    local n=1
    while [[ -f "$out" ]]; do
        out="$MEDIA_DIR/${base}_${n}.mp4"
        (( n++ ))
    done
    echo "$out"
}

# ---------- per-file processor ------------------------------------------------

process_file() {
    local input="$1"

    if [[ ! -f "$input" ]]; then
        err "'$input' is not a file — skipping"
        return 1
    fi

    echo ""
    info "Processing: $input"

    # --- probe ---
    local codec profile height fps_raw fmt pix_fmt color_tx audio_count
    codec=$(probe "$input" codec_name)
    profile=$(probe "$input" profile | tr '[:upper:]' '[:lower:]')
    height=$(probe "$input" height)
    fps_raw=$(probe "$input" r_frame_rate)
    pix_fmt=$(probe "$input" pix_fmt)
    fmt=$(probe_format "$input")
    color_tx=$(probe "$input" color_transfer)
    audio_count=$(audio_stream_count "$input")

    info "  codec=$codec  profile=$profile  height=${height}p  fps=$fps_raw  audio_streams=$audio_count  pix_fmt=$pix_fmt  color_transfer=$color_tx"

    # --- decide what's needed ---
    local needs_reencode=0
    local reasons=()

    [[ "$codec" != "h264" ]] && { needs_reencode=1; reasons+=("codec=$codec (need h264)"); }
    [[ "$profile" != "baseline" && "$profile" != "main" ]] && { needs_reencode=1; reasons+=("profile=$profile (need baseline or main)"); }
    [[ "$pix_fmt" != "yuv420p" ]] && { needs_reencode=1; reasons+=("pix_fmt=$pix_fmt (need yuv420p)"); }
    [[ -n "$height" && "$height" -gt 720 ]] && { needs_reencode=1; reasons+=("height=${height}p > 720p max"); }
    is_hdr "$input" && { needs_reencode=1; reasons+=("HDR color transfer ($color_tx) → tonemapping required"); }
    fps_is_acceptable "$fps_raw" || { needs_reencode=1; reasons+=("fps=$fps_raw (need 24 or 30)"); }

    # Container: ffprobe may report "mov,mp4,..." for .mp4 files — only flag if truly non-mp4
    if [[ "$fmt" != *"mp4"* ]]; then
        needs_reencode=1; reasons+=("container=$fmt (need mp4)")
    fi

    # Audio only needs a remux strip (no full reencode) unless something else does
    local needs_audio_strip=0
    [[ "$audio_count" -gt 0 ]] && needs_audio_strip=1

    # --- build output path ---
    local input_abs
    input_abs="$(cd "$(dirname "$input")" && pwd)/$(basename "$input")"
    local media_abs
    media_abs="$(cd "$MEDIA_DIR" && pwd)"

    # Don't touch a file that is already the output (already in media dir)
    local in_media_dir=0
    [[ "$(dirname "$input_abs")" == "$media_abs" ]] && in_media_dir=1

    local output
    output="$(output_path "$input")"

    # --- act ---
    if [[ $needs_reencode -eq 0 && $needs_audio_strip -eq 0 ]]; then
        info "  File already meets spec."
        if [[ $in_media_dir -eq 1 ]]; then
            info "  Already in media folder — nothing to do."
            return 0
        fi
        info "  Copying → $output"
        cp "$input_abs" "$output"
    elif [[ $needs_reencode -eq 0 && $needs_audio_strip -eq 1 ]]; then
        info "  Only audio strip needed — remuxing (stream copy)."
        info "  → $output"
        ffmpeg -hide_banner -loglevel warning \
            -i "$input" \
            -c:v copy \
            -an \
            -movflags +faststart \
            "$output"
    else
        for r in "${reasons[@]}"; do
            warn "  re-encode reason: $r"
        done

        # Scale filter: downscale to 540p if taller, keep even width
        local vf_args=""
        if [[ -n "$height" && "$height" -gt 540 ]]; then
            vf_args="-vf scale=-2:540"
        fi

        # HDR tonemapping (uses ffmpeg's built-in tonemap; requires the input
        # to be decoded to a linear-light format first via format=gbrpf32le)
        if is_hdr "$input"; then
            warn "  HDR source detected — applying basic tonemap (hable). Review output brightness."
            local hdr_filter="format=gbrpf32le,tonemap=hable,format=yuv420p"
            if [[ -n "$vf_args" ]]; then
                vf_args="-vf scale=-2:540,${hdr_filter}"
            else
                vf_args="-vf ${hdr_filter}"
            fi
        fi

        # fps: force to 30 if not already 24/30 compatible
        local fps_arg=""
        fps_is_acceptable "$fps_raw" || fps_arg="-r 30"

        info "  Re-encoding → $output"
        ffmpeg -hide_banner -loglevel warning -stats \
            -i "$input" \
            -c:v libx264 \
            -profile:v main \
            -level:v 3.1 \
            -preset slow \
            -crf 20 \
            -pix_fmt yuv420p \
            -an \
            -movflags +faststart \
            $fps_arg \
            $vf_args \
            "$output"
    fi

    info "  Output: $output"

    # --- delete original if the output is a different file ---
    if [[ "$input_abs" != "$output" ]]; then
        rm "$input_abs"
        info "  Deleted original: $input_abs"
    fi
}

# ---------- entry point -------------------------------------------------------

if [[ $# -eq 0 ]]; then
    echo "Usage: $(basename "$0") <file> [file ...]"
    echo ""
    echo "Converts video to EcopunkVideoCollage spec and places it in:"
    echo "  $MEDIA_DIR"
    exit 1
fi

if ! command -v ffmpeg &>/dev/null || ! command -v ffprobe &>/dev/null; then
    err "ffmpeg and ffprobe are required. Install with: brew install ffmpeg"
    exit 1
fi

mkdir -p "$MEDIA_DIR"

fail_count=0
for f in "$@"; do
    process_file "$f" || (( fail_count++ )) || true
done

echo ""
if [[ $fail_count -eq 0 ]]; then
    info "Done. All files processed."
else
    warn "Done. $fail_count file(s) failed."
    exit 1
fi
