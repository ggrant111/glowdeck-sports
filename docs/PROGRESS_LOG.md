# GlowDeck GameDay — Progress Log

The ongoing roadmap and checkbox acceptance tracker live in [`../PROJECT_PLAN.md`](../PROJECT_PLAN.md). Add dated entries here after each meaningful change, milestone or hardware test. **Do not mark a test verified without evidence.**

## 2026-11-01 — Sprint 1 source baseline, v0.1

**Completed (code authored):** PlatformIO CYD project, landscape touchscreen score/lights/setup UI, Wi-Fi captive setup, NHL current-score polling adapter, WLED mDNS discovery and manual-IP fallback, manual team-color controls, portable domain logic, native simulator, seven Unity tests, automated CI workflow, developer README and validation checklist.

**Verified:** A desktop `g++` simulation passed: five frames generated **2 display-only scoring alerts** and **0 WLED automatic actions**. No device was connected and no live endpoint was checked in this verification.

**Not yet verified:** PlatformIO ESP32 compile, PlatformIO native tests, GitHub Actions run, touchscreen pins/calibration, first-boot Wi-Fi portal, live NHL feed on device, WLED discovery and manual color change on a real controller.

**Intentionally deferred:** automatic game-driven WLED effects, deduplication beyond portable score-change baseline, snapshot/restore, configurable TV delay, extra leagues, multi-WLED scenes, OTA, hardened TLS and secure administration.

**Next:** Run GitHub Actions, fix any compile errors; bring up the board and a WLED device; record each acceptance-test result in `docs/VALIDATION.md`. Only then close Sprint 1.

## Entry template (copy for every new work session)

### YYYY-MM-DD — Sprint N / version

- **Changed:**
- **Verified (commands, build URL, hardware):**
- **Failed / blockers:**
- **Plan changes / decisions:**
- **Next actions:**
