# Changelog

All notable changes to this project are documented here.

Format follows [Keep a Changelog](https://keepachangelog.com/). GitHub Release notes are generated from commits and merged PRs only — edit a release on GitHub if you want a custom summary.

## Beta releases (`v3.0.0-beta.N`)

Pushes to `main-update` publish a GitHub pre-release; `N` increments automatically (`v3.0.0-beta.1`, `v3.0.0-beta.2`, …).

Stable releases use **Actions → Release → Run workflow** with a semver like `3.0.0`.

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
