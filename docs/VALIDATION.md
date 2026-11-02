# Validation log — v0.1

## Verification results

- PASS: `scripts/check-native.sh` compiled the simulator with system g++ using `-Wall -Wextra -Werror` and executed it successfully.
- PASS IN GITHUB ACTIONS (2026-11-01): 7/7 PlatformIO native Unity tests, `pio run -e cyd` firmware compilation, and firmware artifact upload ([run 41364609955](https://github.com/ggrant111/glowdeck-sports/actions/runs/41364609955)). The current local sandbox still does not have PlatformIO.
- NOT RUN: actual CYD and WLED hardware testing.

## Automated checks available

- Portable core: 7 Unity tests under PlatformIO `env:native`.
- Native C++ deterministic simulator: 5 game frames, exactly 2 display alerts, 0 automatic WLED commands.
- GitHub Actions workflow: intended to compile/test all commits and upload compiled firmware as an artifact.

## Not verified in this environment

- **Physical flash / boot** (firmware has compiled in GitHub Actions, but has not been uploaded to a real CYD).
- Physical ILI9341 TFT and XPT2046 touch mapping/calibration for a specific board revision.
- First-boot captive portal and reliable 2.4 GHz Wi-Fi connectivity.
- Live internet NHL HTTPS redirect/JSON payload response at runtime.
- WLED mDNS discovery and actual LED state change in a user's LAN.

## Known limitations

- Scoreboard only (not goal scorer, power plays or major plays). NHL updates are polling-based, not realtime push.
- Up to 16 NHL games/day displayed (4 per screen). Up to 8 WLED devices saved; first 3 shown on touchscreen; remaining devices can be managed on web UI but not selected from touchscreen in v0.1.
- TLS certificate validation is disabled for NHL proof-of-concept requests.
- WLED API controls segment 0 only, no segment backup/restore and no automatic score effects.
- The default touch transform is approximate. Device-specific calibration is required.
- Wi-FiManager setup blocks normal input while portal is active; after timeout the UI recovers and Wi-Fi can reconnect.
- Local web setup lacks authentication/HTTPS; do not port-forward it.

## v0.2 milestone (2026-11-01)

- PASS: [CI run 41368433335](https://github.com/ggrant111/glowdeck-sports/actions/runs/41368433335) — 17/17 native Unity cases, ESP32 PlatformIO compile and firmware artifact upload.
- PASS: desktop v0.2 mock goal cycle, including restore.
- NOT RUN: CYD display, WLAN, actual NHL response and real WLED state capture/restore. See [v0.2 validation](VALIDATION_V02.md).

## Fire TV test APK v0.1 — separate Android application (2026-11-01)

- PASS: Java-only smoke checks for GoalTracker initial baseline, deduplication, one new goal and RFC1918 IP restrictions.
- PASS: [GitHub Actions run 41372267341](https://github.com/ggrant111/glowdeck-sports/actions/runs/41372267341) — Android SDK, native JVM unit tests, debug APK compile, artifact + release steps succeeded.
- NOT RUN: Fire TV launcher/D-pad test, actual NHL HTTPS requests from Fire OS, actual WLED LAN communications, effect restoration while interrupted, real-live-game reactions.
- The APK is a temporary debug-signed prerelease. Full hardware acceptance required before making claims about live event or WLED reliability.

- PASS (final Fire TV v0.1 build): [CI run 41372660218](https://github.com/ggrant111/glowdeck-sports/actions/runs/41372660218) completed native JVM tests, Android APK compile, checksum and test-release publishing. [Download APK](https://github.com/ggrant111/glowdeck-sports/releases/download/firetv-v0.1.0-test/GlowDeck-GameDay-FireTV-v0.1.apk). WLED preview can now use UTA blue/BOS gold without a selected game. Fire TV/WLED hardware acceptance still NOT RUN.
