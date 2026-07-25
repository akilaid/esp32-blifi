# ADR 0004 - Monorepo layout & tooling

- **Status:** Accepted (2026-07-23)
- **Related:** [`../../README.md`](../../README.md)

## Context

The project ships four independently-versioned artifacts - the `blifi` ESP-IDF
component, the `Blifi` Arduino library, the `blifi` Flutter package, and the
demo app - that must evolve together (a protocol change touches firmware and the
Flutter package at once). They use very different toolchains (ESP-IDF/CMake,
Arduino, Dart/Flutter).

## Decision

Keep everything in **one repository using plain folders**, with **no monorepo
management tool** (no Melos, Nx, Turborepo). Each artifact is **independently
versioned** with its own `CHANGELOG.md` under semantic versioning. Each artifact
lives under its own top-level tree (`firmware/`, `arduino/`, `flutter/`).

## Consequences

- Atomic, reviewable changes to the protocol and both implementations in a single
  commit/PR; the reference docs and code stay in lockstep.
- No extra tooling layer to learn or maintain, and nothing that assumes a single
  language toolchain across such different stacks.
- Coordination is by convention (independent CHANGELOGs, per-artifact CI
  workflows) rather than enforced by a tool - acceptable at this scale.

## Alternatives considered

- **Melos / Nx / Turborepo:** built for many like-toolchain packages; here the
  firmware and Flutter toolchains are too dissimilar for a unifying tool to add
  value proportional to its overhead. Rejected.
- **Separate repositories per artifact:** would make coordinated protocol changes
  span multiple PRs and complicate keeping firmware ↔ package ↔ spec in sync.
  Rejected.
