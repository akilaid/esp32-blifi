# ADR 0007 - Automated release + publish on merge to main

- **Status:** Accepted (2026-07-25)
- **Supersedes:** the manual (`workflow_dispatch`) release gate in
  [`0006-arduino-distribution-mirror.md`](0006-arduino-distribution-mirror.md)
  (the `arduino-release` step) and the earlier manual component/Flutter publish gates.
- **Related:** [`0006-arduino-distribution-mirror.md`](0006-arduino-distribution-mirror.md)

## Context

Each of the three published artifacts (ESP-IDF component, Flutter package, Arduino
library) previously published through a different manual trigger: `component-publish`
and `arduino-release` were `workflow_dispatch` buttons, and the Flutter package was
published by hand-pushing a `blifi-v*` tag. The intent was a "human decides when"
gate. In practice this was error-prone: a version got bumped but never published, and
publishing meant remembering three different manual steps in the right order.

Desired model: **the version bump IS the release**. Bump an artifact's version (and
its CHANGELOG) in a PR; merging that PR to `main` builds, releases, and publishes that
artifact automatically. No manual workflow triggers.

Two platform constraints shape the design:

1. `espressif/upload-components-ci-action` is **idempotent** - it reads the version
   from `idf_component.yml` and silently skips a version that already exists, so it is
   safe to run on every push to `main`.
2. **pub.dev refuses to publish from a branch push** - it only authorises a publish
   triggered by a git **tag** matching its configured pattern (`blifi-v{{version}}`).
   And a tag pushed by the default `GITHUB_TOKEN` does **not** trigger another
   workflow. So auto-publishing Flutter on merge requires a workflow to push the
   release tag using a **PAT**.

## Decision

Drive every release off a version bump merged to `main`; each publisher is idempotent
so an unbumped merge is a harmless no-op.

- **ESP component** - `component-publish.yml` runs the upload action on push to `main`
  (real upload; skips if the version already exists) and on pull requests as
  `dry_run` (validates the manifest and server-side example processing before merge).
- **Arduino library** - unchanged `arduino-mirror.yml` syncs the mirror on push to
  `main`; `arduino-release.yml` now runs automatically via `workflow_run` after a
  successful mirror sync and self-guards (tags the mirror only when
  `library.properties` is newer than the mirror's latest tag).
- **Flutter package** - `flutter-autotag.yml` runs on every push to `main`; if the
  current `pubspec.yaml` version has no `blifi-v<version>` tag yet, it creates and
  pushes that tag with the `RELEASE_PAT`, which fires the existing tag-triggered
  `flutter-package-publish.yml` (OIDC to pub.dev). It self-heals a version that was
  bumped but never tagged.

**Auth / one-time manual setup:** a new fine-grained PAT **`RELEASE_PAT`** with
`contents: write` scoped to `akilaid/esp32-blifi`, added as a repo secret. Required
because a `GITHUB_TOKEN`-pushed tag would not trigger the Flutter publisher.
(`IDF_COMPONENT_API_TOKEN` and `ARDUINO_MIRROR_PAT` are unchanged.)

## Consequences

- To release any artifact: bump its manifest `version` + CHANGELOG in a PR, merge to
  `main`. Publishing happens on its own, in order, for whichever artifacts changed.
- Safe by construction: all three registries reject duplicate versions and every
  publisher is idempotent, so a mistaken or version-less merge publishes nothing
  rather than causing damage.
- Trade-off: the "human decides when" gate is gone. It is replaced by the fact that a
  version bump is an explicit, reviewed edit - you do not bump unless you mean to
  release - plus the PR `dry_run` for the component. Accepted deliberately.
- The one-time `arduino/library-registry` submission is still a separate manual step
  (see ADR 0006); this ADR only automates versions after that.

## Alternatives considered

- **Keep manual `workflow_dispatch` gates:** the status quo that caused the missed
  publish. Rejected per the goal above.
- **Publish Flutter directly from the main-push job (no tag):** impossible - pub.dev
  requires a tag-triggered run. Rejected.
- **Configure pub.dev environment-based auth to avoid the tag:** pub.dev's environment
  option is additive to the tag requirement, not a replacement, so it would not remove
  the tag/PAT need. Rejected as extra config for no benefit.
- **Auto-tag with `GITHUB_TOKEN` instead of a PAT:** a tag it pushes does not trigger
  the publish workflow. Rejected.
