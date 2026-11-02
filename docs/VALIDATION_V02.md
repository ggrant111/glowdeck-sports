# v0.2 verification evidence

- Date: 2026-11-01.
- Offline `./scripts/check-native.sh` PASS with g++ -std=c++17 -Wall -Wextra -Werror: old scoreboard simulation emits two display-only alerts, v0.2 reaction replay starts one fake goal scene and restores original state exactly once.
- Native unit tests: 17 test cases executed locally with a minimal Unity-compatible *temporary shim* for assertion logic. Official PlatformIO Unity result must be recorded separately from the CI run; the shim is not a replacement for official Unity or target compile.
- PASS: [GitHub Actions run 41368433335](https://github.com/ggrant111/glowdeck-sports/actions/runs/41368433335) completed successfully: 17/17 native Unity tests, `pio run -e cyd` firmware compile, and `GlowDeck-GameDay-v0.2-firmware` artifact uploaded.
- Hardware: **NOT RUN** (CYD touchscreen not yet obtained).
- Network / NHL / WLED: **NOT RUN** on hardware, simulated logic only.
- Browser UI: preview at `simulator/web/index.html`; mock inputs only, not actual firmware pixels or timing.

See `docs/SPRINT2.md` for hardware matrix and `docs/VALIDATION.md` for retained v0.1 baseline evidence.

- PASS (latest `main`): [CI run 41368809822](https://github.com/ggrant111/glowdeck-sports/actions/runs/41368809822) — 17/17 Unity cases, CYD firmware compile, artifact upload. Includes explicit WLED segment-0 enable/unfreeze fix. No hardware testing claimed.

- Hosted UI smoke verification: [GlowDeck GameDay Simulator](https://glowdeck-gameday-simulator.lovable.app) was tested by the separate Lovable browser build at an iPhone-sized viewport with game selection, arming and simulated Utah goal; this is a separate web mock and not CYD firmware verification. Attachment viewers can block local inline JS.

## Live integration bridge — separate from ESP32 firmware (2026-11-01)

- PASS: `python -m unittest discover -s tools/tests -v` — 9/9 standard-library unit cases with a fake WLED; live-NHL response normalization and restoration safety covered.
- Added GitHub Actions `.github/workflows/bridge-tests.yml` to verify these cases on pushes affecting `tools/**`.
- Live NHL scores at the documented public endpoint were observed via browsing; the hosted simulator uses a restricted same-origin proxy. This does not verify the ESP32's HTTPS implementation.
- NOT RUN: actual home WLED or real iPhone-to-LAN connection. Physical preview and automatic restoration must be checked from the user's trusted home network.
- NOT PROVIDED: automatic real NHL events triggering actual lights through the local bridge (manual preview only).

- PASS: [Local bridge CI run 41370774438](https://github.com/ggrant111/glowdeck-sports/actions/runs/41370774438) — nine Python 3.11 offline tests passed. Also manually tested local HTTP server: HTML served, unauthenticated API rejected with 403, valid access code returned configured WLED status. **Not** a real-WLED connectivity test.
