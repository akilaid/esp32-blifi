# ADR 0006 - Arduino distribution via a generated mirror repo

- **Status:** Accepted (2026-07-24)
- **Related:** [`../plan.md`](../plan.md) §7, §7.1;
  [`0003-single-codebase-for-arduino.md`](0003-single-codebase-for-arduino.md);
  [`0004-monorepo-layout.md`](0004-monorepo-layout.md)

## Context

`arduino/Blifi/` lives inside this monorepo (per ADR 0003/0004 - one codebase, the
Arduino library is a thin wrapper over the ESP-IDF component). But the two Arduino
distribution channels are indexed very differently:

- **PlatformIO Registry** is explicit-publish (`pio pkg publish`, reads
  `library.json`) and doesn't care where the manifest lives - a monorepo subfolder
  is fine.
- **Arduino Library Manager** (built into the Arduino IDE) is a **crawler**, not an
  explicit-publish system. Its hard requirements: the repository is submitted once as
  a PR to `arduino/library-registry`, `library.properties` **must sit at the repo
  root** (it will not descend into a monorepo subfolder), and new versions are picked
  up only from **git tags** that look like semver.

So a subfolder of `esp32-blifi` cannot be indexed by the Arduino Library Manager.

## Decision

Keep `arduino/Blifi/` as the single source of truth, and publish a **generated,
root-flattened mirror** at
[`akilaid/esp32-blifi-arduino`](https://github.com/akilaid/esp32-blifi-arduino) -
nothing there is written by hand. Two **separate** GitHub Actions workflows:

1. **`arduino-mirror.yml`** - continuous, low-stakes. On push to `main` filtered to
   `arduino/Blifi/**`, it `rsync`s the *contents* of `arduino/Blifi/` to the mirror
   root and pushes a commit. Excludes the `.development` marker (its presence blocks
   Library Manager indexing per the Arduino Library spec) and local build output. The
   mirror's `README.md` is rebuilt as a "generated mirror" banner
   (`arduino/mirror-README-banner.md`) + the library's own README. Keeps the mirror
   current for install-from-Git users; ships nothing to Library Manager users by
   itself.
2. **`arduino-release.yml`** - reads `version` from `library.properties`; if it's
   newer than the mirror's latest `v*` tag, it tags that commit and creates a GitHub
   Release. That tag is what makes a new version available to every Arduino IDE user.
   *(Originally a manual `workflow_dispatch`; now runs automatically after each mirror
   sync per [ADR 0007](0007-automated-release-on-merge.md). It still self-guards on the
   version, so it only releases when `library.properties` was actually bumped.)*

**Auth:** the default `GITHUB_TOKEN` cannot push to a *different* repository, so both
workflows use a **fine-grained PAT** (`ARDUINO_MIRROR_PAT`) with `contents: write`
scoped to *only* the mirror repo.

## Consequences

- The monorepo stays the one place to edit; Arduino IDE users still get a spec-compliant
  root-level library. The mirror is disposable/regenerable.
- **Manual one-time setup** (not automated):
  1. Create the empty `akilaid/esp32-blifi-arduino` repo (with a default branch).
  2. Create a fine-grained PAT (`contents: write`, this repo only) and add it as the
     `ARDUINO_MIRROR_PAT` secret in `akilaid/esp32-blifi`.
  3. Once the library is judged ready for real users, submit the **one-time** PR to
     `arduino/library-registry` adding the mirror's URL. *(Deliberately deferred - not
     part of this phase.)*
- Two moving repos to reason about, but the coupling is one-directional and generated.

## Alternatives considered

- **Point the Library Manager at a monorepo subfolder:** impossible - the indexer
  requires `library.properties` at the repo root. Rejected.
- **One workflow that mirrors and tags on every push:** would ship a release to every
  Arduino IDE user on every merge to `main`. Rejected in favour of a manual release
  gate.
- **Move the Arduino library to its own top-level repo (no monorepo):** breaks the
  single-codebase decision (ADR 0003) and splits coordinated protocol changes across
  repos. Rejected.
