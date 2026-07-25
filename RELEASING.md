# Releasing

This repository is a monorepo that ships **three independently versioned
artifacts** from one codebase:

| Artifact | Source | Published to |
|----------|--------|--------------|
| ESP-IDF component | [`firmware/components/blifi/`](firmware/components/blifi) | [ESP Component Registry](https://components.espressif.com/components/akilaid/blifi) (`akilaid/blifi`) |
| Flutter package | [`flutter/packages/blifi/`](flutter/packages/blifi) | [pub.dev](https://pub.dev/packages/blifi) (`blifi`) |
| Arduino library | [`arduino/Blifi/`](arduino/Blifi) | Arduino Library Manager (via the [generated mirror](https://github.com/akilaid/esp32-blifi-arduino)) |

Releases are **automated**: publishing happens on merge to `main`, driven entirely
by version bumps. There are no manual publish buttons and no tags to push by hand.
The design rationale lives in
[ADR 0007](docs/adr/0007-automated-release-on-merge.md) and
[ADR 0006](docs/adr/0006-arduino-distribution-mirror.md).

## The model

- **Trunk-based.** `main` is always releasable; work happens on branches and lands
  through pull requests.
- **The version bump is the release.** To publish an artifact, bump its manifest
  version (and add a changelog entry) in a PR. When that PR merges to `main`, the
  matching workflow publishes it.
- **Independent versions.** Each artifact follows [SemVer](https://semver.org)
  on its own timeline. Bump only the artifact(s) you actually changed.
- **Merging without a version bump publishes nothing.** Every publisher is
  idempotent and the registries reject re-uploading an existing version, so a merge
  that does not change a version is a safe no-op.

## Cutting a release

1. **Branch from `main`** and make your changes (with tests where it makes sense).
2. **For each artifact you intend to release**, in the same PR:
   - bump the `version` in its manifest (see the table below), and
   - add a section to that artifact's `CHANGELOG.md`.
3. **Open a pull request** and wait for CI to pass. For component changes, the
   `component-publish` job runs a registry **dry-run** on the PR that validates the
   manifest before anything is published.
4. **Merge to `main`.** The relevant workflow publishes automatically.
5. **Verify** the new version appears (see [Verifying a release](#verifying-a-release)).

### Version fields

| Artifact | Bump `version` in | Add a section to |
|----------|-------------------|------------------|
| ESP-IDF component | `firmware/components/blifi/idf_component.yml` | `firmware/components/blifi/CHANGELOG.md` |
| Flutter package | `flutter/packages/blifi/pubspec.yaml` | `flutter/packages/blifi/CHANGELOG.md` |
| Arduino library | `arduino/Blifi/library.properties` (`version=`) | `arduino/Blifi/CHANGELOG.md` |

The demo app (`flutter/apps/demo_app`) is not published and is versioned separately;
it needs no release step.

## Versioning policy

Standard [Semantic Versioning](https://semver.org). While an artifact is pre-1.0
(`0.y.z`):

- **patch** (`0.1.3 -> 0.1.4`): bug fixes, documentation, internal changes.
- **minor** (`0.1.x -> 0.2.0`): new features, or a breaking change (breaking changes
  bump the minor while below 1.0).
- **major** (`0.x -> 1.0.0`): the first release that commits to a stable public API.

Keep the three artifacts' versions aligned when a change spans all of them, but this
is a convention, not a requirement.

## What happens on merge

Each artifact has its own path from "merged to `main`" to "published":

- **ESP-IDF component** - [`component-publish.yml`](.github/workflows/component-publish.yml)
  runs on push to `main`, reads `version` from `idf_component.yml`, and uploads. The
  upload is idempotent (an existing version is skipped). Because the component's
  dependency graph (NimBLE + mbedTLS/PSA) makes the registry's server-side example
  processing slow, the upload step is best-effort and a follow-up step polls the
  registry API: the job is green only once the version is actually live.
- **Flutter package** - pub.dev only authorizes a publish triggered by a git tag, so
  [`flutter-autotag.yml`](.github/workflows/flutter-autotag.yml) runs on push to
  `main` and, if the current `pubspec.yaml` version has no `blifi-v<version>` tag yet,
  creates and pushes it. That tag triggers
  [`flutter-package-publish.yml`](.github/workflows/flutter-package-publish.yml),
  which publishes to pub.dev over OIDC.
- **Arduino library** - [`arduino-mirror.yml`](.github/workflows/arduino-mirror.yml)
  syncs `arduino/Blifi/` into the root-flattened mirror repo on push to `main`, then
  [`arduino-release.yml`](.github/workflows/arduino-release.yml) runs automatically
  and tags the mirror when `library.properties` is newer than the mirror's latest tag.
  The Arduino Library Manager indexes the new tag on its next crawl.

## Verifying a release

- **Component:** <https://components.espressif.com/components/akilaid/blifi>
- **Flutter:** <https://pub.dev/packages/blifi>
- **Arduino:** the mirror's [releases](https://github.com/akilaid/esp32-blifi-arduino/releases);
  the Library Manager index updates within a day of a new tag.

You can also watch the run in the repository's **Actions** tab after merging.

## Maintainer setup (one-time)

The automation depends on three repository secrets. These are already configured;
they are listed here for maintainers who fork or re-create the repo.

| Secret | Scope | Used by |
|--------|-------|---------|
| `IDF_COMPONENT_API_TOKEN` | ESP Component Registry token (`write:components`) | `component-publish` |
| `ARDUINO_MIRROR_PAT` | Fine-grained PAT, `contents: write` on the mirror repo | `arduino-mirror`, `arduino-release` |
| `RELEASE_PAT` | Fine-grained PAT, `contents: write` on this repo | `flutter-autotag` (a tag pushed by the default `GITHUB_TOKEN` would not trigger the publish workflow) |

Flutter publishing also requires automated publishing to be enabled for the package
on pub.dev (Admin tab, repository `akilaid/esp32-blifi`, tag pattern
`blifi-v{{version}}`).

## Troubleshooting

- **Nothing published after merging.** The version was not bumped, or the change did
  not touch that artifact's paths. Bump the version in a follow-up commit and merge.
- **`component-publish` shows a red "Upload" step but the job is green.** Expected:
  the upload action times out on slow example processing while the version publishes
  a moment later. The confirm step is the real gate.
- **A publisher fails with "version already exists".** That version was already
  released. Bump to a new version.
