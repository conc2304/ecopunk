# blob-preset-coverage

This is a predeploy check. It verifies that every production video has a Blob detection preset in `assets/shared/blob/blob-detection-profiles.json`.

## Where it runs

- `scripts/deploy.sh` runs it automatically through `scripts/check-blob-detection-presets.sh`.
- You can also run it on its own:

```sh
scripts/check-blob-detection-presets.sh            # gate (prompts on missing presets)
tools/blob-preset-coverage/blob_preset_coverage    # report only; exit 0 PASS / 1 WARNING / 2 ERROR
```

## Results

| Result | Meaning | Deploy behavior |
|---|---|---|
| PASS | Every production video has a preset | Continues with no prompt |
| WARNING | Some videos have no preset. At runtime those videos use the canonical detector defaults | Asks `Continue deployment? [y/N]` (default No). Non-interactive: stops unless `--allow-missing-blob-presets` is given. `--dry-run` only reports |
| ERROR | The preset file is unreadable or invalid, or the media root is missing | Stops. This cannot be overridden |

Presets whose video no longer exists are listed as notes. They are never deleted.

## How media are identified

Video IDs come from production code, never from a second implementation:
- `MediaCatalog::build()` and `synthesizeMediaId()` give the same IDs ExperienceRuntime uses;
- `BlobDetectionProfileStore` parses the preset file.

Only the directory scan is mirrored, because the production scan (`ofDirectory`) needs openFrameworks. The scan rules are: the top level of `assets/shared/media`, `.mp4` in any letter case, and no hidden files.

## Tests

`make test` runs the core tests and the deploy-gate tests. Both use temporary fixtures, never the real assets.

## Adding a new video

1. Run `blob-region-prototype` and navigate to the video (`n`/`p`).
2. Tune the detector and save (`s`).
3. Rerun the deploy. The check passes.
