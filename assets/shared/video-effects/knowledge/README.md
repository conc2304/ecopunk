# Canonical Shared Effect Knowledge root

This directory is the **sole canonical AUTHORED root** for the Shared
Effect Knowledge pack (DEC-016, Shared Effects Architecture-Closure
Session). It is analogous to `assets/shared/media/` for Shared Video: one
physical source of truth, reached by every consuming sketch via a
relative-path traversal from its own `bin/data/`, never via a new symlink.

- **Authoring**: `shader-effect-debugger`'s "Export Knowledge Pack" action
  (`ofApp::exportKnowledgePack()`) writes directly here —
  `effect-knowledge-pack.json`, containing every accumulated
  whitelist/blacklist `KnowledgeEntry` plus `EffectLevelKnowledge`
  effect-level defaults. This is the only writer.
- **Distribution**: `scripts/sync-video-effect-assets.py` copies (does not
  symlink) this file into each consuming sketch's
  `bin/data/shared-video-effects/knowledge/effect-knowledge-pack.json` —
  the same synced-assets convention already used for shader source files.
  Per-sketch copies under `bin/data/` are derived/deployment artifacts
  only; never hand-edit them.
- **Consumption**: scenes/sketches import their synced copy via
  `videoeffects::importEffectKnowledgePack(...)` (see
  `EffectKnowledgePack.h`). `temporal-fields`' `TFEffectPicker::setup()` is
  the first real consumer.

Do not hand-copy this file into a sketch's `bin/data/`. Re-run the sync
script (or a full sketch build, which invokes it) instead.
