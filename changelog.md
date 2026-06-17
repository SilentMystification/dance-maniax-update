# Changelog

All notable changes to this project are documented here.

Format follows [Keep a Changelog](https://keepachangelog.com/).

**Workflow:**

1. Add bullets under **`## Unreleased`** while you work.
2. Push code to **`main-update`** — CI publishes that section on the GitHub Release (if not empty).
3. After the release, **you** move those lines to a version heading (e.g. `## v3.0.0-beta.10`) and clear **`## Unreleased`**. Include that edit in your next commit (markdown-only pushes do not trigger a release).

CI never commits changelog changes — no extra bot commits to pull.

## Beta releases (`v3.0.0-beta.N`)

Pushes to `main-update` publish a GitHub pre-release; `N` increments automatically (`v3.0.0-beta.1`, `v3.0.0-beta.2`, …).

Stable releases use **Actions → Release → Run workflow** with a semver like `3.0.0`.

## Unreleased

_Add new changes here. Only this section is included in the next release notes._

## v3.0.0-beta.9

Clean up release process

## v3.0.0-beta.1 – v3.0.0-beta.6

_Shipped during early v3 pre-releases. Kept for history; not included in future release notes._

### Added / changed

- Judgement display
- Advanced results screen
- Login functionality in Continuous Mode

### Audio

- Optional native ASIO support
- Global offset in operator menu
- Per-player offset in profile
- Wall-clock synchronization of audio playback to reduce jitter

### Phoenix IO

- Enhanced extio firmware at 115200 baud (vs 9600 baud)
- IO evaluated per game tick instead of ~4 ms serial round-trip

### Options & UI

- Option files added
- Song select options menu for P1 and P2
- Animated gold selector; dark purple item outlines
- Bobbing gold triangle when editing; blue triangles for nav arrows
- Settings menu sound effects (open/close, scroll, value change)
- "Upside-Down" mirror renamed to "V-Flip"

### Fixed

- Center + Mirror: notes rendered at outer columns instead of center columns (2–5)
