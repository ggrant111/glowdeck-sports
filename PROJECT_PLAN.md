# GlowDeck GameDay — Living Project Plan

> **Canonical roadmap and progress tracker**  
> **Project:** GlowDeck GameDay · **Hardware:** ESP32-2432S028R ("Cheap Yellow Display" / CYD, 2.8-inch resistive touchscreen)  
> **Repository:** https://github.com/ggrant111/glowdeck-sports  
> **Owner:** Geoff · **Created:** 2026-11-01 · **Last reviewed:** 2026-11-01  
> **Current phase:** Sprint 1 hardware validation **blocked until the CYD arrives**; Sprint 2 **v0.2 source, native tests, ESP32 compilation and firmware artifact VERIFIED in CI**, physical acceptance pending.

## 0. How to keep this plan current

This file is the **source of truth** for milestones and acceptance criteria. Update it whenever a feature, test, risk, decision, or target changes.

- Use `[x]` for **verified complete**, `[ ]` for **not verified/not complete**, and **In progress / Blocked / Planned** for milestone status.
- Never mark firmware **Build passed** merely because code exists. Link a successful GitHub Actions run or attach a local build log.
- For each sprint, update its checklist, **Evidence / test notes**, **Open blockers**, and **Next milestone**.
- Add an entry to `docs/PROGRESS_LOG.md` for substantive changes. Keep `docs/VALIDATION.md` for test evidence and unresolved hardware issues.
- Save firmware changes and plan changes together where practical. Suggested commit prefix: `sprint-N:`, `fix:`, `docs:`, `test:`.
- A proposed future behavior is **not** an implemented feature merely because it appears in this plan.

## 1. Vision and boundaries

A standalone, touch-controlled GameDay console that reads live sports scores and game events and controls one or more local WLED devices in team colors and event-specific light scenes — **no always-on computer or server**. Start with NHL and extend to other leagues via interchangeable sports-data adapters.

**User journey:** power on CYD → connect Wi-Fi → discover/select WLED devices or zones → choose league / team / game → show live scoreboard → optionally arm safe, delayed light reactions for scoring and important plays → restore the lights when reactions end.

### MVP capabilities (target v1.0)

- [ ] Reliable touchscreen interface: scoreboard, game detail, selected team/game, light settings, setup/status.
- [ ] First-time Wi-Fi onboarding with recovery path; Wi-Fi reconnection.
- [ ] NHL schedule and live score updates with clear errors/staleness indicators.
- [ ] Discover or manually pair multiple WLED devices and select targets.
- [ ] Team colors and safe, configurable goal/event light actions.
- [ ] Prevent duplicate triggers or triggering on initial sync/score corrections.
- [ ] Configurable **TV/game stream delay from 0 to 90 seconds**.
- [ ] Save and restore prior WLED state on scene completion and on recovery.
- [ ] Brightness cap, effect throttling, no high-frequency flashes, manual disable/override.
- [ ] Hardware-validated release with repeatable simulation tests and documented install/update procedure.

**Out of scope for initial v0.1 source:** unattended goal reactions, power-play and final effects, multi-controller zones, OTA, third-party leagues, external music streaming, FFT/audio-reactive playback. These are future features.

## 2. Baseline snapshot — November 1, 2026

### Implemented in the v0.1 Sprint 1 source package

- [x] PlatformIO project scaffold for ESP32-2432S028R (ILI9341 + XPT2046 touch).
- [x] 320×240 landscape touchscreen UI code with score, light and setup screens.
- [x] Wi-Fi captive-portal onboarding and local web setup for direct-IP WLED pairing.
- [x] NHL daily score polling adapter (60-second interval in v0.1).
- [x] WLED `_wled._tcp` discovery, basic status/validation and manual team-color commands for segment 0.
- [x] Portable game-state and score-change logic isolated from ESP32 hardware.
- [x] Deterministic desktop simulator and seven PlatformIO Unity test cases authored.
- [x] GitHub Actions firmware/test workflow authored.
- [x] Build, touch-calibration, WLED safety and acceptance documentation authored.
- [x] Desktop C++ simulator **executed successfully**: two display-only goal alerts; **zero** automatic WLED calls.

### Still pending verification

- [x] GitHub Actions / PlatformIO native Unity tests pass (7/7, [run 41364609955](https://github.com/ggrant111/glowdeck-sports/actions/runs/41364609955)).
- [x] ESP32 `pio run -e cyd` firmware build passes ([CI run](https://github.com/ggrant111/glowdeck-sports/actions/runs/41364609955)).
- [ ] Upload and boot on an actual ESP32-2432S028R.
- [ ] Screen orientation, resistive touch corners and calibration confirmed.
- [ ] Wi-Fi onboarding, reconnect and persisted settings confirmed on hardware.
- [ ] Live NHL HTTPS/JSON requests confirmed on the ESP32.
- [ ] WLED mDNS scan and/or direct IP discovery confirmed on a real LAN.
- [ ] Manual HOME/AWAY commands change intended WLED segment and only that segment.
- [ ] WLED settings persist after restart and **no automatic LED commands** occur on score updates.

**Important:** The simulator validates only portable game logic; it does not prove firmware compilation, display pins, API connectivity, network discovery or LED behavior. NHL prototype TLS uses `setInsecure()`; this is a release blocker.

## 3. Original seven-week roadmap, translated to tracked milestones

The **week numbers are sequence targets, not calendar commitments**; advance only after the relevant test gate passes.

| Week | Milestone / version | Planned deliverable | Test gate | Status |
|---|---|---|---|---|
| 1 | **Sprint 1 — v0.1 Foundation** | Touch UI, Wi-Fi, NHL scoreboard, WLED discovery/manual colors, portable simulator and CI | Firmware build, board smoke, one WLED device, no auto triggers | **CI verified, board acceptance blocked** |
| 2 | **Sprint 2 — v0.2 Goal engine** | Selected-game tracking, conservative score-delta goals, arm/disarm, 0–90s delay, crash-safe state snapshot/restore | 17 native tests; firmware CI; physical WLED restore pending | **Software/CI passed; hardware blocked** |
| 3 | **Sprint 3 — v0.3 NHL events** | Goal celebrations, power plays, intermissions, final wins/losses; reliable game-state transitions | NHL fixture replays + live game event comparisons | Planned |
| 4 | **Sprint 4 — v0.4 Lighting setup** | Select multiple WLED targets/zones, team-color mapping, event templates, brightness/quiet controls | Multi-device sync and individual device failure tests | Planned |
| 5 | **Sprint 5 — v0.5 Resilience** | Polling schedule, stale-data/offline behavior, memory/performance, security and device configuration improvements | 2-hour stress tests and reconnection/failure injection | Planned |
| 6 | **Sprint 6 — v0.9 Release candidate** | Hardware integration, setup polish, regression fixes, field-test documentation | **24-hour soak + 3 live NHL games** | Planned |
| 7 | **Sprint 7 — v1.0 Release** | Stable documented firmware, flash/update instructions, locked acceptance suite and version tag | All MVP gates met; reproducible CI build | Planned |

### Sprint 1 — hardware bring-up (DEFERRED until board arrives)

**Definition of done:** Every item below verified on a CYD unit (not just simulated).

- [x] GitHub Actions workflow passed on 2026-11-01: [run 41364609955](https://github.com/ggrant111/glowdeck-sports/actions/runs/41364609955).
- [x] PlatformIO ESP32 build produced the `GlowDeck-GameDay-v0.1-firmware` Actions artifact.
- [ ] Flash successful; display visible and landscape oriented.
- [ ] Calibrate touch using real four-corner raw readings.
- [ ] Set up Wi-Fi using on-device portal; show IP on SETUP.
- [ ] NHL scoreboard shows current games, or a useful error on game-free days.
- [ ] Pair WLED by discovery or local IPv4 web setup.
- [ ] Tap HOME/AWAY to change selected WLED's segment 0, and OFF to switch it off.
- [ ] Verify no automatic WLED send on a new score.
- [ ] Power cycle, verify Wi-Fi and saved WLED list retained.

**Known limits:** Only first three of eight saved WLED devices shown in touch selection; up to 16 games/day, four shown per screen; approximate default touch mapping; web config restricted to trusted LAN; manual colors overwrite segment 0 state (no automatic restoration yet).

**Evidence / test notes:** Desktop simulator passed on 2026-11-01. GitHub Actions passed 7/7 native tests and compiled/uploaded an ESP32 firmware artifact on 2026-11-01 ([evidence](https://github.com/ggrant111/glowdeck-sports/actions/runs/41364609955)). Physical device tests **NOT RUN**.

**Open blockers:** Need a real CYD board/WLED validation session; do not classify this release as hardware-verified yet.

**Next milestone:** Complete Sprint 1 hardware test checklist and record results.

### Sprint 2 — safe automatic reaction engine (v0.2 SOFTWARE VERIFIED)

- [x] Select a specific live game, favorite home/away side, and WLED target; arm only by explicit action; every boot starts disarmed.
- [ ] Add authoritative game/play event-ID processing. **Not yet implemented:** v0.2 uses conservative scoreboard deltas, not confirmed NHL goal identities (planned v0.3).
- [x] Suppress first-frame goals, duplicate scores, corrections and ambiguous multi-goal jumps; auto-disarm on ambiguity.
- [x] Adjustable 0–90s delay (15s increments on UI), cancel on disarm, stale feed or game switch.
- [x] WLED segment-0 and master state snapshot saved to NVS before auto effect; restore after 10s/abort/reboot with retry after Wi-Fi recovery. **Hardware unverified.**
- [x] Single pending event and 20s cooldown suppress overlapping goals; static solid team colors only.
- [x] Simulator and **17 native Unity tests** cover duplicate polls, score correction, missed polls, delays, restart, disconnected restores and timer wrap.
- [x] [GitHub Actions run 41368433335](https://github.com/ggrant111/glowdeck-sports/actions/runs/41368433335): 17/17 native tests passed, `pio run -e cyd` compiled and `GlowDeck-GameDay-v0.2-firmware` uploaded.
- [x] Added hardware-free four-page browser mock at `simulator/web/index.html`.
- [ ] Hardware gate: successful automatic WLED capture/apply/restore, UI touch controls and power-cycle recovery on a physical ESP32-2432S028R.

**Latest main-branch verification:** [CI run 41368809822](https://github.com/ggrant111/glowdeck-sports/actions/runs/41368809822) — 17/17 native tests, ESP32 firmware build and artifact upload passed after final WLED segment-enable fix.\n\n**Sprint 2 evidence:** [Successful CI run](https://github.com/ggrant111/glowdeck-sports/actions/runs/41368433335) on 2026-11-01. See [docs/SPRINT2.md](docs/SPRINT2.md) and [docs/VALIDATION_V02.md](docs/VALIDATION_V02.md). **Physical acceptance remains blocked solely by unavailable hardware and unverified LAN/API integrations.**

### Sprint 3 — NHL event policies

**Proposed defaults; user-adjustable, subject to real-game validation:**

| Event | Proposed reaction | Duration |
|---|---|---|
| Favorite-team goal | Team-color celebration | 10s |
| Opponent goal | Brief opposing-color pulse | 3s |
| Power play | Alternating team colors, gentle rate | While event active |
| Intermission | Dim or neutral scene | While intermission active |
| Favorite-team win | Victory scene, then restore | 20s |
| Score correction | No celebration; refresh data only | None |

- [ ] Validate data availability for goals, period status, power plays and finals.
- [ ] Ensure event classification is data-driven, not inferred solely from the latest score.
- [ ] Provide disabled/no-op fallback when event details are unreliable.
- [ ] No rapid flashing or strobes; brightness ceiling and cool-down enforcement.

### Sprint 4 — multi-WLED controls and user experience

- [ ] Discover and label controllers; permit selecting all, a group, or individual targets.
- [ ] Apply team colors across selected devices, gracefully handling offline devices.
- [ ] Add optional zone/segment targeting if RAM/network budget allows.
- [ ] Preview each WLED target safely; save profiles per favorite team.
- [ ] Make armed/disarmed state and active-event timer obvious on device.
- [ ] Define compatibility strategy with existing GlowDeck Android app; **no requirement for that app to be running**.

### Sprint 5 — network, performance and safety

- [ ] Adaptive NHL polling target: ~15 minutes outside active play, ~60 seconds near game start, ~20 seconds for live games, within provider rate and device constraints. **v0.1 currently polls every 60 seconds.**
- [ ] Exponential backoff with jitter and last-updated timestamp when API or Wi-Fi fails.
- [ ] Proper certificate validation for HTTPS, no embedded Wi-Fi passwords/API secrets.
- [ ] Prevent setup endpoints from exposing private configuration on untrusted networks.
- [ ] Add firmware/update recovery approach appropriate to flash capacity (OTA only if safe).
- [ ] Test memory stability, network latency, watchdog recovery and stale-event handling.

### Sprints 6–7 — release candidate and v1.0

- [ ] Repeat all Sprint 1–5 hardware acceptance tests.
- [ ] Replay score/event sequences with a WLED simulator and real devices.
- [ ] Perform a 24-hour connected soak test.
- [ ] Observe **at least three real NHL games**, noting feed delays, missed/duplicate triggers and restore correctness.
- [ ] Publish firmware build artifacts, release notes, hardware pin map and troubleshooting.
- [ ] Tag `v1.0.0` only after acceptance criteria are met.

## 4. Test intervals (from original blueprint)

| When | Duration | What to verify |
|---|---|---|
| Each small change | **5–10 minutes** | Native tests, brief device smoke, touch input, expected WLED target |
| Each completed milestone | **30–60 minutes** | Full UI→NHL→event logic→WLED integration and failure/restore checks |
| Every week | **2 hours** | Device stability, Wi-Fi drops, WLED disconnects, stale-feed recovery |
| Before v1.0 | **24 hours** | Continuous connected soak, reconnects, memory stability |
| Before v1.0 | **3 live games** | Delays, event dedupe, real goals/power plays, correct event effects |

Record **date, device revision, firmware commit, test, expected result, actual result, outcome, and evidence** in `docs/VALIDATION.md`.

## 5. Planned post-v1.0 expansions

- **v1.1:** follow multiple concurrent NHL games and favorite-team priority.
- **v1.2:** league adapters for **NFL, MLB, NBA and college sports**; league-specific events and colors.
- **v1.3:** optional interoperability with GlowDeck Android (profiles, scene definitions, multi-WLED zones), while CYD retains stand-alone operation.
- **v2.x:** custom animations, scoreboard dashboards, user-designed events; optional audio/music/FFT features assessed separately based on hardware/memory/legal feasibility.

## 6. Architecture and constraints

```text
NHL API / future league adapters
              |
        Sports data adapter
              |
         Game model + event detector ----> deterministic simulator/tests
              |
   safe rule engine + optional delay
              |
    WLED target selector / scheduler
              |
      WLED local JSON / later DDP
              |
         Wi-Fi LED controllers

  CYD UI <--> saved device config / favorites / arming state
```

**Architecture rules:** retain a portable event/state engine separate from network, UI and hardware; no mandatory cloud/backend; minimize WLED traffic; protect existing segments/presets; do not trigger on uncertain data; manual control works independently of sports feeds.

## 7. Risks, decisions and open questions

| ID | Item | Impact / mitigation | State |
|---|---|---|---|
| R-01 | CYD hardware pin / screen / touch variants | Board bring-up and four-corner calibration first | Open |
| R-02 | Public NHL endpoints may change or rate-limit | Adapter boundary, bounded requests, last-known-good state | Open |
| R-03 | TV delay can exceed API timing | Configurable 0–90s reaction delay; test with real broadcasts | Planned |
| R-04 | WLED state overwritten by event effects | Snapshot/restore and strict target scope in Sprint 2 | Open |
| R-05 | Network/mDNS isolation | Manual-IP fallback on LAN | Mitigation coded, untested |
| R-06 | HTTPS prototype skips certificate verification | Block production release until verified TLS | Open |
| R-07 | False goals / duplicate events | Event identity, baseline suppression, dedupe and correction tests | Planned |
| D-01 | NHL first, other leagues after reliable NHL release | Keep league adapter interface modular | Accepted |
| D-02 | v0.1 is **manual-only** for LEDs | No surprise automatic reactions before restore engine | Accepted |
| D-03 | Device must function without a continuously running PC/server | Local Wi-Fi + WLED communication | Accepted |

## 8. Change-control and tracking convention

When updating:
1. Update the relevant sprint checklist and status table.
2. Add one entry to `docs/PROGRESS_LOG.md` with **date, version, what changed, tests, next action**.
3. Update `docs/VALIDATION.md` with proof of build/hardware checks.
4. If the scope changes, revise acceptance criteria rather than silently marking a feature done.
5. Keep the README focused on installation; link here for the long-term roadmap.

### Immediate priority queue

1. **DONE** — Established CI, passed 7/7 native tests, compiled ESP32 firmware, and uploaded artifact.
2. **P0** — Flash the CYD; calibrate touch orientation and test Wi-Fi.
3. **P0** — Verify NHL response and one WLED device on the local LAN.
4. **P1** — Fix any pin/library/network faults discovered on device.
5. **DONE (software)** — Sprint 2 v0.2 goal engine, simulator and CI firmware build complete. **NEXT:** hardware acceptance once ESP32 arrives; then event-ID-powered Sprint 3.

## 9. Pre-hardware integration bench (2026-11-01)

The touchscreen has not arrived; Sprint 1/2 **physical acceptance remains blocked**. To reduce risk while hardware is unavailable:

- [x] Added a **Demo / Live NHL** toggle to the separate [hosted GameDay simulator](https://glowdeck-gameday-simulator.lovable.app) with a server-side fixed-endpoint NHL scores proxy and an optional play-by-play game-ID endpoint (provider/phone behavior may vary).
- [x] Implemented **local-only Python 3 integration bench** under [tools/LOCAL_BRIDGE.md](tools/LOCAL_BRIDGE.md). It retrieves live NHL scores and offers read-only WLED state inspection and **manual 2–10 second solid team-color previews** on one private-LAN WLED controller with a journaled restore.
- [x] Nine deterministic local bridge unit tests pass against a fake WLED (NHL parsing, snapshot/restore, restart recovery, failure journal retention, validation).
- [x] [Python bridge GitHub Actions CI run 41370774438](https://github.com/ggrant111/glowdeck-sports/actions/runs/41370774438) passed; local server authentication smoke test also passed.
- [ ] Validate actual NHL requests and **real WLED preview/restore** from the user's home network with the local bridge and iPhone on the same Wi-Fi.
- [ ] Confirm hosted Live NHL mode from the user's Safari and measure freshness and empty-game handling.
- [ ] Do not enable **automatic** WLED control from real NHL feeds in this bridge; this is a deliberately manual integration test. The ESP32 v0.2 opt-in engine still awaits hardware acceptance.
- [ ] When the CYD arrives, use compiled v0.2 firmware and the original Sprint 1/2 board acceptance matrices.

**Safety**: Never expose the bridge port to the public internet. Use an explicit private WLED IP, local access token, brightness cap, snapshot before preview, restore on timeout, and recovery journal after interruption. Initial live scoring observations are not proof of automatic goal-event identity; do not graduate to autonomous lights until event IDs and deduplication are verified.

## 10. Fire TV hardware-free NHL → WLED integration test console (2026-11-01)

With the CYD touchscreen still unavailable, we have added a **native Fire TV APK**, separate from the ESP32 firmware, so a real TV and WLED can test most of the NHL/network/event and lighting integration path.

- [x] Native Android Java Fire TV application under [firetv/](firetv/README.md), installable via ADB and navigable with remote D-pad.
- [x] NHL score list from public `/v1/score/now` and play-by-play from `/v1/gamecenter/{gameId}/play-by-play`.
- [x] Manual private-LAN WLED pairing via IPv4 and name/version/status read from `/json/info`.
- [x] Read WLED state and preview home/away team colors for eight seconds, with a crash-recovery restore journal.
- [x] Opt-in live goal monitor with event-ID baseline, favorite-team selection, 0–90-second TV delay, one scene at a time, cooldown, and foreground-only arming. Default **DISARMED**; never arm automatically at boot.
- [x] Native Java goal-ID/dedupe and address validation tests compiled locally; Android GitHub Actions compiled debug APK and passed unit tests. [First successful APK CI](https://github.com/ggrant111/glowdeck-sports/actions/runs/41372267341).
- [ ] Download and sideload on an actual Fire TV, validate D-pad navigation and the real NHL API response.
- [ ] Confirm WLED connection and 8-second state restore on a noncritical LED controller; test manual restore after simulated outage.
- [ ] Observe three real NHL games for new goal-ID detection, timing, disarm reliability and false positive suppression; log results before considering production automatic lights.

**Latest Fire TV verification:** [GitHub Actions run 41372660218](https://github.com/ggrant111/glowdeck-sports/actions/runs/41372660218) passed Java tests, signed APK assembly and release upload, including standalone UTA/BOS WLED previews on NHL game-free days. App remains **hardware-untested**.\n\n**Test APK:** [GitHub prerelease](https://github.com/ggrant111/glowdeck-sports/releases/tag/firetv-v0.1.0-test). **Warning:** CI debug signing is ephemeral, so a future replacement APK can require uninstall/reinstall. Fire TV app foreground/lifecycle tests are **not** a substitute for ESP32 hardware bring-up.

## 11. Fire TV v0.2 background GameDay monitor (2026-11-01)

**Why:** v0.1 deliberately disarmed whenever the Activity left the foreground, so NHL-to-WLED goal reactions could not continue when watching a game in YouTube TV, ESPN, Prime Video or another TV app. This v0.2 update moves network polling, event safety and WLED effects to an Android **foreground Service** controlled by the TV Activity.

**Release:** [Fire TV v0.2 test APK](https://github.com/ggrant111/glowdeck-sports/releases/download/firetv-v0.2.0-test/GlowDeck-GameDay-FireTV-v0.2.apk) · [source and installation guide](firetv/README.md) · [verified CI run 41373942957](https://github.com/ggrant111/glowdeck-sports/actions/runs/41373942957)

- [x] Service declared with Android foreground-service permissions and `dataSync` type, promoted immediately via `startForeground()`.
- [x] Explicit **ARM IN BACKGROUND** action; never automatically resume after boot or process death (`START_NOT_STICKY`).
- [x] Ongoing foreground notification with user STOP action and direct return-to-app action; on-screen STOP button works after returning from streaming apps.
- [x] Activity `onStop()` no longer disarms NHL goal monitoring; foreground service owns polling and delayed effects independently of UI visibility.
- [x] Initial NHL play-by-play baseline, 25-second periodic polling, ~55-second live-game status checks, ID dedupe, favorite-team ownership, 20-second cooldown, 0–90s TV delay.
- [x] When network polling fails, PAUSE and cancel delayed triggers; establish a new baseline after recovery rather than replaying old goals.
- [x] WLED state journaling before any goal scene, 8-second automatic restoration, explicit STOP restoration attempt, and journal retained if restore fails.
- [x] Block manual WLED changes, game switches, or delay changes while monitoring is active.
- [x] Seven additional JVM safety rule tests and portable Java smoke assertions; APK built and released by [CI](https://github.com/ggrant111/glowdeck-sports/actions/runs/41373942957).
- [x] **Initial real Fire TV acceptance (2026-11-01):** app sideloaded; user confirmed GAME / EVENTS displays ARMED after switching to YouTube TV, and Fire TV `dumpsys` reported `GameDayService` foreground with notification ID 1227 and roughly 7m41s service elapsed. Notification action and 30-minute endurance remain untested.
- [ ] **Switching test:** arm for an actual live NHL matchup; press Home and watch another streaming app at least 30 minutes; confirm polling/logs and goal detection continue.
- [x] **Initial WLED manual test (2026-11-01):** user reports the real eight-second WLED color test works. Exact saved state restoration and automatic goal-driven effects still need explicit validation.
- [ ] **Lighting test:** verify exact WLED segment 0 and master state restored after an actual new favorite goal, including a deliberate STOP during an active scene.
- [ ] **Recovery test:** disconnect Wi-Fi during polling; confirm PAUSED state, no old goals replay on reconnect; force-stop/relaunch and check any outstanding restore journal.
- [ ] **Endurance:** observe at least 3 live NHL games on a real Fire TV across relevant Fire OS versions; record API latency, duplicate/missed goals and service lifecycle failures.

**Known caveats:** The Fire TV must remain powered and network-connected; OS memory/sleep restrictions may stop the service; Android 15+ has special limits for `dataSync` with newer target SDKs; debug APK signatures can differ between builds and require uninstall/reinstall. No 100% uptime guarantee or live-device validation has been claimed.
