# GlowDeck GameDay — Progress Log

The ongoing roadmap and checkbox acceptance tracker live in [`../PROJECT_PLAN.md`](../PROJECT_PLAN.md). Add dated entries here after each meaningful change, milestone or hardware test. **Do not mark a test verified without evidence.**

## 2026-11-01 — Sprint 1 source baseline, v0.1

**Completed (code authored):** PlatformIO CYD project, landscape touchscreen score/lights/setup UI, Wi-Fi captive setup, NHL current-score polling adapter, WLED mDNS discovery and manual-IP fallback, manual team-color controls, portable domain logic, native simulator, seven Unity tests, automated CI workflow, developer README and validation checklist.

**Verified:** A desktop `g++` simulation passed: five frames generated **2 display-only scoring alerts** and **0 WLED automatic actions**. No device was connected and no live endpoint was checked in this verification.

**Not yet verified:** PlatformIO ESP32 compile, PlatformIO native tests, GitHub Actions run, touchscreen pins/calibration, first-boot Wi-Fi portal, live NHL feed on device, WLED discovery and manual color change on a real controller.

**Intentionally deferred:** automatic game-driven WLED effects, deduplication beyond portable score-change baseline, snapshot/restore, configurable TV delay, extra leagues, multi-WLED scenes, OTA, hardened TLS and secure administration.

**Next:** Run GitHub Actions, fix any compile errors; bring up the board and a WLED device; record each acceptance-test result in `docs/VALIDATION.md`. Only then close Sprint 1.

### 2026-11-01 — Repository sync and CI proof

- **Changed:** Published all 18 Sprint 1 source and documentation files, plus `PROJECT_PLAN.md`, to `ggrant111/glowdeck-sports` main. Fixed PlatformIO native test linking (`test_build_src = yes`), pinned the upstream touchscreen library at commit `f956c5d`, and made `UiAction` constructible under the ESP32 project's C++ mode.
- **Verified:** [GitHub Actions run 41364609955](https://github.com/ggrant111/glowdeck-sports/actions/runs/41364609955) succeeded: **7/7 native tests passed**, **ESP32 CYD firmware compiled**, and firmware build artifact **GlowDeck-GameDay-v0.1-firmware** was uploaded.
- **Failed / blockers:** Earlier CI runs initially exposed a missing native test source link, a nonexistent library version, and touch/UI compile incompatibilities; all fixed. **Physical ESP32 + WLED acceptance remains untested.**
- **Plan changes / decisions:** Sprint 1 source and CI gates checked off. No automatic goal lighting until Sprint 2 safety/state restoration is complete.
- **Next actions:** Flash the compiled firmware on an ESP32-2432S028R, calibrate touch, connect Wi-Fi, verify live NHL data, pair one WLED controller, confirm manual colors/power-off and persistence.

### 2026-11-01 — Sprint 2 v0.2 software milestone

- **Changed:** Introduced portable conservative goal engine with explicit arm/disarm, favorite HOME/AWAY selection, 0–90-second broadcast delay, stale-score and correction fail-closed handling, 20-second cooldown and solid 10-second team color. Added WLED snapshot-before-effect and NVS reboot recovery, four-tab GAME UI, on-device local web controls, and a hardware-free browser demonstration.
- **Verified:** Desktop C++ goal simulator produced 1 team-color scene + 1 restoration. All **17/17 native Unity tests passed**, and ESP32 firmware compiled and uploaded as `GlowDeck-GameDay-v0.2-firmware` in [GitHub Actions run 41368433335](https://github.com/ggrant111/glowdeck-sports/actions/runs/41368433335).
- **Blocked:** No physical CYD board yet; touchscreen calibration, live NHL API polling, WLED journal/restore on real hardware and Wi-Fi recovery remain untested. Scoreboard-based events are not authoritative NHL goal-event IDs.
- **Plan changes / decisions:** Keep v0.1 hardware acceptance open, advance v0.2 software/CI gates. Do not auto-arm after reboot, use only segment 0 and static color until real hardware validation.
- **Next actions:** Flash and accept v0.1/v0.2 when CYD arrives. Next software sprint: v0.3 NHL event-ID source, power-play and intermission policy behind safety gates.

## Entry template (copy for every new work session)

### YYYY-MM-DD — Sprint N / version

- **Changed:**
- **Verified (commands, build URL, hardware):**
- **Failed / blockers:**
- **Plan changes / decisions:**
- **Next actions:**
