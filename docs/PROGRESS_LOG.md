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

### 2026-11-01 — v0.2 final main-branch build verified

- Added explicit WLED segment 0 enable and effect unfreeze before a solid goal color, with prior state still journaled for recovery.
- [GitHub Actions run 41368809822](https://github.com/ggrant111/glowdeck-sports/actions/runs/41368809822) PASSED on `main`: 17/17 tests, CYD compile, firmware artifact uploaded. No physical device acceptance yet.

### 2026-11-01 — Hosted v0.2 simulator for iPhone

- **User-reported issue:** downloaded HTML simulator opened inside the iOS attachment viewer but inline JavaScript did not render the touchscreen or make its buttons functional.
- **Resolution:** built and published a separate [mobile-friendly browser simulator](https://glowdeck-gameday-simulator.lovable.app) via Lovable. Added a GitHub README launch link and a visible file-preview fallback/warning. This is **not firmware** and makes no real WLED or NHL requests.
- **Verification:** simulator project reported successful browser interaction at iPhone width (select UTA@BOS → arm → UTA goal), no horizontal scroll; its separate unit tests passed. Physical ESP32 verification remains pending.
- **Next:** use the hosted link for simulator feedback before buying/receiving hardware.

### 2026-11-01 — Live NHL feeds and local real-WLED testing path

- **Changed:** Hosted simulator now has a Demo/Live NHL toggle and restricted server-side proxy, while retaining MOCK ONLY WLED effects. Added `tools/gameday_bridge.py`, a local mobile-sized interactive NHL/WLED test interface, a detailed setup guide, and GitHub Actions Python unit tests. This is a computer-hosted integration bench separate from the ESP32 firmware.
- **Verified:** Nine offline Python tests against fake WLED passed, including live-score normalization, segment 0 snapshot/restore, restart recovery, and retain-and-retry after failed restoration. WLED state writes require explicit manual confirmation and are limited to 2–10 seconds.
- **Not verified:** No real user's WLED or home LAN reached by this session; no proof of physical device control or iPhone access yet. Hosted Lovable build reported its own Live NHL fetch and test checks; user Safari verification pending.
- **Next:** Verify the hosted Live NHL toggle on iPhone, run the local bridge on a trusted home PC, select a real game and test a short WLED color preview plus scene restoration. Then pursue v0.3 NHL goal-event IDs.

### 2026-11-01 — Local bridge HTTP and GitHub Actions verification

- **Verified:** [GitHub Actions local bridge workflow 41370774438](https://github.com/ggrant111/glowdeck-sports/actions/runs/41370774438) passed all 9 Python offline tests. A local smoke run served the HTML interface, rejected unauthorized API access (HTTP 403), and accepted a valid access code for GET `/api/status`.
- **Still pending:** Test on user's iPhone/home computer and actual WLED LAN; local test had no real WLED contact.

### 2026-11-01 — Fire TV native NHL/WLED test APK created

- **Changed:** Added `firetv/` standalone Android TV / Fire TV app (D-pad UI, NHL scores and play-by-play, WLED IP pairing and JSON API, solid team-color previews with restore journal, optional explicit real-goal arming). Added .github/workflows/firetv-apk.yml automated Android build, JVM tests and prerelease APK upload.
- **Verified:** Pure Java tests compiled/executed locally; [GitHub Actions run 41372267341](https://github.com/ggrant111/glowdeck-sports/actions/runs/41372267341) passed Android unit testing, APK compilation, checksum and release publishing. Debug APK is a true signed sideloadable package. No real Fire TV / WLED hardware was available to validate behavior.
- **Next actions:** Sideload the APK on Fire TV, confirm TV remote focus, NHL connection, direct WLED GET/POST, scene restoration, and optional live-goal event test. Record evidence in validation log.

### 2026-11-01 — Fire TV test APK ready to sideload

- Fixed Android SDK CI invocation for GitHub runner and updated GitHub prerelease to the correctly named `GlowDeck-GameDay-FireTV-v0.1.apk`.
- Added independent Utah-blue/Boston-gold WLED color previews with no NHL matchup, so manual LAN tests work on game-free days.
- [CI run 41372660218](https://github.com/ggrant111/glowdeck-sports/actions/runs/41372660218) passed Android unit tests, compiled and published the signed debug APK. Physical Fire TV, live NHL and real WLED tests remain open.

## Entry template (copy for every new work session)

### YYYY-MM-DD — Sprint N / version

- **Changed:**
- **Verified (commands, build URL, hardware):**
- **Failed / blockers:**
- **Plan changes / decisions:**
- **Next actions:**
