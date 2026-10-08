# Architecture Acceptance — HUD Skin Manifest Final Schema Alignment

**Owner:** Architecture & Program Coordination
**Status:** ACCEPTED — bounded tools-only implementation
**Date recorded:** 2026-10-08
**Recorded by:** HUD Runtime coding-agent session, at the repository owner's instruction. This session records the grant and makes no acceptance decision of its own.
**Work item:** [HUD-004](03-work-registry.md#hud-004--skin-package-specification-and-skin-loader-contract). This is sub-evidence only and does not complete HUD-004.

> **Provenance.** Architecture issued this acceptance in the project conversation. No copy existed in the repository or `~/Downloads` until this file. The repository owner relayed it on 2026-10-08.
>
> Each block below is copied **exactly as relayed**. Nothing has been paraphrased or merged. Text outside the blocks only identifies a relay or reconciles it against in-repo evidence.
>
> If the original Architecture document is later recovered, it supersedes this record wherever the two differ, and the difference must be reported. It must not be silently merged.

## 1. Acceptance decision

Relayed title and decision:

> Architecture Acceptance — HUD Skin Manifest Final Schema Alignment
>
> ACCEPTED — Bounded tools-only implementation.

Relayed basis, in the owner's words:

> It accepted the four-rule alignment based on the reported 394/394 normal and sanitizer tests.

Relayed documentation disposition:

> Documentation status: Accepted by Architecture; durable record still needs filing.

This file is that durable record.

## 2. Dimension-field requiredness (closed)

Relayed confirmation:

> Architecture explicitly confirmed:
> - expectedWidth is mandatory and must equal 1280.
> - expectedHeight is mandatory and must equal 720.
> - Both apply to every declared shell, decoration, and emissive asset.

This matches the implemented behavior. The open question raised in the alignment return ("required vs equal-if-present") is **closed**.

## 3. What was accepted (reconciled against the repository)

- **Scope:** the four-rule Final Schema Alignment of the nonproduction manifest validator in `tools/hud-skin-manifest/`:
  - no leading zeros in `skinVersion` components;
  - each component ≤ 4294967295;
  - mandatory 1280×720 logical dimensions on every declared full-canvas asset;
  - consecutive internal hyphens permitted in `skinId`.
- **Evidence (desktop, macOS, as reported by the coding agent):**
  - `make -f Makefile.tests test` → 394/394, 0 warnings;
  - ASan/UBSan build with `-Wpedantic` → 394/394, 0 warnings, exit 0.
- **Earlier increments:**
  - The isolated foundation was provisionally accepted (`~/Downloads/HUD-Skin-Foundation-Architecture-Authorization-and-Rulings.md`, untracked).
  - Hardening went through `~/Downloads/HUD-Skin-Foundation-Hardening-Architecture-Acceptance-Handoff.md` (untracked).
  - The alignment was authorized by `~/Downloads/HUD-Skin-Manifest-Final-Schema-Alignment-Authorized-Coding-Agent-Prompt.md` (untracked).

## 4. What this acceptance does not establish

- A Skin Package v1 freeze. The Loader Contract v1 is a first draft only.
- Canonical Blueprint compatibility. HUD-006 is unresolved, and the expected Blueprint ID and version remain explicit caller inputs with no default.
- Image decoding, alpha correctness, or physical texture dimensions.
- Atlas consumption or icon roles. Atlas stays temporarily rejected; optional atlas remains a future direction.
- A production loader (HUD-005), ExperienceRuntime or Validation Studio integration, or `HudServices`/`HudAssetPaths`.
- GPU residency, or any Raspberry Pi evidence.
- Authorization for production integration.
