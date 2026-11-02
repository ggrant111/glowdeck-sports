# GlowDeck GameDay — Cross-Platform Feature Parity

**Canonical feature agreement for Fire TV, ESP32-2432S028R (CYD), and browser demo.**

This file must be reviewed with **every new feature** and **every behavior change**. The Fire TV APK and ESP32 firmware must follow the same game/goal/lighting behavior; their screens and operating-system lifecycles will necessarily differ. This is a feature parity contract, not proof of identical hardware performance.

## v0.3 behavior contract

| Shared behavior | Fire TV v0.3 | ESP32 v0.3 | Browser simulator |
|---|---|---|---|
| Live NHL daily scores | Implemented | Implemented | Hosted live, local mock |
| NHL game selection | Implemented | Implemented | Demo selection |
| Celebrate AWAY / HOME / BOTH | Implemented | Implemented on touch and web | Implemented mock and hosted UI update |
| Team scoring colors | 32-team table | Same 32-team table | Relevant mock teams |
| Real goal source | NHL PBP event IDs | NHL PBP event IDs | Simulated goals only; hosted live scores read-only |
| 0–90s user TV delay | 15-second UI increments | 15-second UI increments | Demo values |
| Unique/ambiguous-goal safety | Baseline + dedupe; suppress multi-goal ambiguity | Baseline + increasing event-ID boundary; suppress multi-goal ambiguity | Mock duplicate guard |
| Live game required for arming | Yes | Yes | Fictional live game in mock |
| WLED color + restoration | 8s, persisted snapshot | 8s, persisted NVS snapshot | 8s mock restore only |
| Brightness | Cap 160 | Cap 160 | Mock only |
| Reaction cooldown | 20s | 20s | Mock should reflect this; confirm hosted behavior |
| No auto-arm on reboot | Yes | Yes | Yes |
| Offline feed behavior | Pause + new baseline | Pause + new baseline | Fake offline toggle |
| Runs without separate PC/server | Foreground service | Standalone controller loop | Hosted webpage (different architecture) |
| Live hardware proof | Goals and restore confirmed in Fire TV v0.2 | NOT TESTED — CYD not yet received | Browser demo only |

## Differences that are intentional

- **Fire TV** uses a foreground Android service so a streaming app can remain visible. The ESP32 runs its own main event loop continuously.
- **ESP32** has a 2.8-inch 320x240 resistive touchscreen, mDNS WLED discovery and a local Wi-Fi setup page. Fire TV uses a D-pad layout and manual WLED IP.
- **NHL polling errors:** both systems fail closed and rebaseline; their precise Android/ESP32 network lifecycles may differ. Neither promises guaranteed scoring latency, and both may miss a goal during outages.
- **Browser preview** never communicates with an actual WLED controller. Live NHL information is read-only in hosted preview. Its tests do not validate real NHL goal-triggered lighting.
- **ESP32** currently uses `WiFiClientSecure::setInsecure()`; certificate validation is a release blocker. Hardware touch mapping, RAM stability and WLED JSON restore all need board testing.
- **Do not arm both physical controllers against the same WLED target simultaneously.** Neither provides cross-controller locking; competing restore journals could fight.

## Change-control process (required for future work)

1. Define the user-facing behavior here first, including differences necessitated by hardware.
2. Implement shared behavior in **Fire TV**, **ESP32**, and **browser demo** (where demonstrable). Clearly mark unsupported capabilities, rather than silently omitting them.
3. Update common NHL team colors and goal safety rules together; do not change one table alone.
4. Run `python3 scripts/check-parity.py`; it automatically verifies colors, major v0.3 contract flags, polling intervals, 8-second scenes, delay caps, journal hooks and UI modes in the checked-in implementations. The CI workflow `.github/workflows/parity.yml` runs this across commits.
5. Run platform tests: Fire TV `gradle :app:testDebugUnitTest :app:assembleDebug`; ESP32 `pio test -e native` and `pio run -e cyd`; browser mobile navigation tests.
6. Verify both relevant GitHub Actions jobs pass **before** merging new feature work to `main`.
7. Update `PROJECT_PLAN.md`, `docs/VALIDATION.md`, and `docs/PROGRESS_LOG.md`. Mark software/CI completion distinctly from hardware validation.
8. Test each platform in the field. Track differences as explicit follow-up tickets; never claim physical parity from CI alone.
9. Publish compatible APK and firmware artifacts with independent versions; no need to delay one platform for unavailable hardware, but never claim full hardware validation.

**Limitations of the parity script:** simple static assertions are guardrails, not exhaustive semantic equivalence. The external Lovable-hosted simulator is not built by GitHub CI; it must be manually reviewed after a hosted deployment.

## Current acceptance gates

- [x] Fire TV background service, actual goal reaction and visible scene restoration on a real WLED (v0.2 baseline).
- [x] Fire TV v0.3 AWAY/HOME/BOTH software build and tests.
- [x] ESP32 v0.3 Away/Home/Both selector and PBP goal engine authored.
- [ ] ESP32 v0.3 build/CI on `main` and physical CYD validation.
- [ ] Fire TV v0.3 BOTH mode observed for opposing goals on a real NHL game.
- [ ] Hosted browser v0.3 deployment and manual iPhone touch check.
- [ ] Real ESP32 NHL TLS behavior, memory, touch alignment, Wi-Fi, and WLED state restoration.
