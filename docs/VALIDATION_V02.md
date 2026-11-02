# v0.2 verification evidence

- Date: 2026-11-01.
- Offline `./scripts/check-native.sh` PASS with g++ -std=c++17 -Wall -Wextra -Werror: old scoreboard simulation emits two display-only alerts, v0.2 reaction replay starts one fake goal scene and restores original state exactly once.
- Native unit tests: 17 test cases executed locally with a minimal Unity-compatible *temporary shim* for assertion logic. Official PlatformIO Unity result must be recorded separately from the CI run; the shim is not a replacement for official Unity or target compile.
- ESP32 compiler / GitHub Actions: **PENDING CI** at time of this initial source snapshot; record successful run URL only after completed.
- Hardware: **NOT RUN** (CYD touchscreen not yet obtained).
- Network / NHL / WLED: **NOT RUN** on hardware, simulated logic only.
- Browser UI: preview at `simulator/web/index.html`; mock inputs only, not actual firmware pixels or timing.

See `docs/SPRINT2.md` for hardware matrix and `docs/VALIDATION.md` for retained v0.1 baseline evidence.
