#!/usr/bin/env bash
#
# prep_textures.sh
#
# Batch-applies the "universal texture prep pipeline" to every image in an
# input folder, using ffmpeg. Produces two outputs per source image:
#
#   1. <name>_tint.png  - desaturated, tonal-range-compressed, contrast-lowered,
#                          softly tinted, slightly blurred flat texture
#   2. <name>_mask.png   - pure grayscale luminance map of the same processing,
#                          suitable for use as an opacity/alpha mask in code
#
# Usage:
#   ./prep_textures.sh <input_dir> <output_dir> [tint]
#
#   tint - optional: warm | cool | green | none  (default: none)
#          warm  -> subtle brown/orange cast   (wood, bark, earth, canopy light)
#          cool  -> subtle blue/white cast     (frost, water, air)
#          green -> subtle green cast          (moss, lichen)
#          none  -> no color cast, pure desaturated gray
#
# Example:
#   ./prep_textures.sh ./raw_textures ./prepped warm
#
# Requires: ffmpeg on PATH.

set -euo pipefail

IN_DIR="${1:-}"
OUT_DIR="${2:-}"
TINT="${3:-none}"

if [[ -z "$IN_DIR" || -z "$OUT_DIR" ]]; then
  echo "Usage: $0 <input_dir> <output_dir> [warm|cool|green|none]"
  exit 1
fi

if ! command -v ffmpeg >/dev/null 2>&1; then
  echo "Error: ffmpeg not found on PATH."
  exit 1
fi

mkdir -p "$OUT_DIR"

# --- Tunable pipeline parameters -------------------------------------------
SATURATION=0.10      # step 1: desaturate to ~10% of original
CONTRAST=0.70         # step 3: lower contrast by ~30%
LEVEL_MIN=0.16        # step 2: compress tonal range - lift black point (~40/255)
LEVEL_MAX=0.84        # step 2: compress tonal range - lower white point (~215/255)
BLUR_SIGMA=1.5        # step 7: slight gaussian blur to prevent moire/aliasing

# Tint amounts (colorbalance shadows/midtones/highlights nudges), subtle only
case "$TINT" in
  warm)  TINT_FILTER="colorbalance=rs=0.06:gs=0.00:bs=-0.06:rm=0.04:gm=0.00:bm=-0.04" ;;
  cool)  TINT_FILTER="colorbalance=rs=-0.05:gs=0.00:bs=0.06:rm=-0.03:gm=0.00:bm=0.04" ;;
  green) TINT_FILTER="colorbalance=rs=-0.04:gs=0.05:bs=-0.03:rm=-0.02:gm=0.03:bm=-0.02" ;;
  none)  TINT_FILTER="" ;;
  *)
    echo "Unknown tint '$TINT' - use warm | cool | green | none"
    exit 1
    ;;
esac

shopt -s nullglob nocaseglob
FILES=("$IN_DIR"/*.png "$IN_DIR"/*.jpg "$IN_DIR"/*.jpeg "$IN_DIR"/*.webp)
shopt -u nocaseglob

if [[ ${#FILES[@]} -eq 0 ]]; then
  echo "No images found in $IN_DIR"
  exit 0
fi

echo "Found ${#FILES[@]} image(s). Tint: $TINT"
echo "----------------------------------------"

for f in "${FILES[@]}"; do
  base="$(basename "${f%.*}")"
  tint_out="$OUT_DIR/${base}_tint.png"
  mask_out="$OUT_DIR/${base}_mask.png"

  echo "Processing: $base"

  # --- Tinted, flattened texture ---
  # 1. eq: desaturate + lower contrast in one pass
  # 2. colorlevels: compress tonal range so nothing hits true black/white
  # 3. colorbalance: subtle tint (skipped if TINT=none)
  # 4. gblur: light blur to soften fine noise / prevent aliasing on downscale
  if [[ -n "$TINT_FILTER" ]]; then
    FILTER_CHAIN="eq=saturation=${SATURATION}:contrast=${CONTRAST},colorlevels=romin=${LEVEL_MIN}:romax=${LEVEL_MAX}:gomin=${LEVEL_MIN}:gomax=${LEVEL_MAX}:bomin=${LEVEL_MIN}:bomax=${LEVEL_MAX},${TINT_FILTER},gblur=sigma=${BLUR_SIGMA}"
  else
    FILTER_CHAIN="eq=saturation=${SATURATION}:contrast=${CONTRAST},colorlevels=romin=${LEVEL_MIN}:romax=${LEVEL_MAX}:gomin=${LEVEL_MIN}:gomax=${LEVEL_MAX}:bomin=${LEVEL_MIN}:bomax=${LEVEL_MAX},gblur=sigma=${BLUR_SIGMA}"
  fi

  ffmpeg -y -loglevel error -i "$f" -vf "$FILTER_CHAIN" "$tint_out"

  # --- Pure grayscale luminance mask (same tonal compression + blur, no tint) ---
  MASK_CHAIN="eq=saturation=0:contrast=${CONTRAST},colorlevels=romin=${LEVEL_MIN}:romax=${LEVEL_MAX}:gomin=${LEVEL_MIN}:gomax=${LEVEL_MAX}:bomin=${LEVEL_MIN}:bomax=${LEVEL_MAX},gblur=sigma=${BLUR_SIGMA},format=gray"

  ffmpeg -y -loglevel error -i "$f" -vf "$MASK_CHAIN" "$mask_out"

done

echo "----------------------------------------"
echo "Done. Output written to: $OUT_DIR"
echo "  *_tint.png -> flattened, tinted texture (composite directly, opacity set in code)"
echo "  *_mask.png -> grayscale luminance map (use as alpha/opacity mask)"
