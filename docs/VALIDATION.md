# Validation log — v0.1

## Local verification result

- PASS: `scripts/check-native.sh` compiled the simulator with system g++ using `-Wall -Wextra -Werror` and executed it successfully.
- NOT RUN: PlatformIO native Unity tests or `pio run -e cyd`. PlatformIO is not installed and installation from PyPI failed due to disabled external network resolution.
- NOT RUN: actual CYD and WLED hardware testing.

## Automated checks available

- Portable core: 7 Unity tests under PlatformIO `env:native`.
- Native C++ deterministic simulator: 5 game frames, exactly 2 display alerts, 0 automatic WLED commands.
- GitHub Actions workflow: intended to compile/test all commits and upload compiled firmware as an artifact.

## Not verified in this environment

- **ESP32 PlatformIO compile** (requires PlatformIO and cached/downloadable espressif32 platform/libraries).
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
