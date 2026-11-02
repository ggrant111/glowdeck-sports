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
- [x] ESP32 v0.3 [build/CI on `main`](https://github.com/ggrant111/glowdeck-sports/actions/runs/41380024037) passed: 23/23 native tests and firmware image built.
- [ ] Physical CYD hardware acceptance still open.
- [ ] Fire TV v0.3 BOTH mode observed for opposing goals on a real NHL game.
- [x] Hosted browser v0.3 deployed at https://glowdeck-gameday-simulator.lovable.app with emulated-mobile test report.
- [ ] Manual iPhone Safari touch check still needed.
- [ ] Real ESP32 NHL TLS behavior, memory, touch alignment, Wi-Fi, and WLED state restoration.

## v0.4 Audio parity implementation (SOURCE IMPLEMENTED, HARDWARE UNVERIFIED)

See [AUDIO_V04.md](AUDIO_V04.md). All user-facing GameDay sound features must ship intentionally on both **ESP32** and **Fire TV**, with a browser mock when applicable. Required behavior: built-in original sound library **and** user-supplied custom WAV clips; independent per-team/event selection; safe default mute, volume and test/preview; same eligible NHL goal identity and TV delay as lighting; audio failure must not interrupt WLED restoration; no hardware auto playback without explicit enabling. Hardware-dependent audio outputs differ (GPIO26 via speaker amplifier on CYD; Android audio with OS focus on Fire TV). **CI audio-parity static checks are now implemented** in `scripts/check-parity.py` and exercised by the GitHub parity workflow.

### Audio parity milestones

- [x] ESP32 v0.4 SOUND screen with built-in Horn/Double/Siren, Custom WAV, off, volume and standalone preview.
- [x] Fire TV v0.4 SOUND tab with same source choices, imported WAV, volume and team-specific preview.
- [x] Local browser mock v0.4 SOUND tab and demo-goal sound playback.
- [x] 16kHz/mono/16-bit/10-second canonical WAV validation and original generated horn samples.
- [x] Safe background audio default OFF and WLED restore independent of audio success.
- [x] New audio source parity check on v0.4 branch passed.
- [ ] Real ESP32 speaker and SD tests, Fire TV audio focus while YouTube TV runs, and iPhone Safari audio preview.
- [ ] Real multiple-goal sustained soak with optional sound and WLED restore timing verified on hardware.
- [ ] Extend audio events beyond NHL goals (power play, intermission, wins).

### Official v0.4 build verification

- [x] [ESP32 CI](https://github.com/ggrant111/glowdeck-sports/actions/runs/41383412590): 28/28 native tests, firmware artifact.
- [x] [Fire TV CI](https://github.com/ggrant111/glowdeck-sports/actions/runs/41383412450): JVM unit tests, debug APK release.
- [x] [Audio parity CI](https://github.com/ggrant111/glowdeck-sports/actions/runs/41383412448): static cross-platform checks passed.
- [ ] Real CYD speaker/amplifier + microSD playback, Fire TV audio mixing in YouTube TV, actual iPhone Safari playback remain to be tested.

## Fire TV v0.4.1 browser sound manager (device-specific delivery)

- [x] Fire TV: local-only paired management server on TCP 8788, authenticated sound settings, all 32 NHL teams, WAV upload and TV speaker preview, source and JVM validations implemented.
- [x] Browser simulator: existing team sounds and user-chosen local WAV preview already available; no cloud upload to an actual controller is claimed.
- [x] ESP32: existing touch and local web sound settings, microSD custom WAV playback and per-team selections remain unchanged.
- [ ] **Parity gap:** secure browser-to-microSD upload directly onto ESP32 is not implemented. Requires a safe SD/touch bus arbitration strategy, physical CYD acceptance and pairing credentials shown on board. Track as v0.4.2/next hardware milestone.
- [ ] **Fire TV hardware acceptance:** test iPhone access, pairing, real WAV upload and playback, then confirm custom horn while YouTube TV is streaming and WLED restores.

Platform-specific network hosting may differ; gameplay and goal/WLED behavior remain shared. **Do not silently equate browser mock or Fire TV LAN upload with proven ESP32 hardware upload.** See [LAN_SOUND_MANAGER_V041.md](LAN_SOUND_MANAGER_V041.md).

### v0.4.1 verification (2026-11-01)

- [x] [Fire TV unit tests + signed debug APK](https://github.com/ggrant111/glowdeck-sports/actions/runs/41386294301) passed on main.
- [x] [Cross-platform source-level audio parity checks](https://github.com/ggrant111/glowdeck-sports/actions/runs/41386294313) passed on main.
- [x] [Unmodified ESP32 firmware regression tests/build](https://github.com/ggrant111/glowdeck-sports/actions/runs/41386294269) passed.
- [ ] Actual Fire TV LAN upload + Safari pairing and TV preview verification remains open.
- [ ] Secure ESP32 browser-to-microSD upload is an explicit follow-up, not part of this Fire TV v0.4.1 release.


## v0.5.0 DDP event effects — 2026-11-02

DDP has been moved ahead of the v1.0 NHL release gate. See [DDP_V05.md](DDP_V05.md) for the exact behavior and acceptance checklist. Fire TV, ESP32 and local browser source implement seven event patterns, opt-in whole-device streaming, event toggles and previews. Real hardware and hosted Lovable parity remain unverified. Existing Fire TV LAN audio upload and ESP32 microSD behavior are preserved.

Local cross-platform static checks, 175 C++/browser pixel vectors, native simulator and nine bridge regression tests passed. Full firmware/JVM/APK CI results will be recorded on the feature PR; source checks alone are not a build or physical validation claim.


## v0.5.0 published build verification — 2026-11-02

Source commit: `e54e752f62823847776f43a7b2bf34df00f67bc3`, branch `feature/ddp-event-effects`, draft PR https://github.com/ggrant111/glowdeck-sports/pull/1 .

- Fire TV Gradle JVM tests, debug APK assembly and checksum verification: **PASS** — https://github.com/ggrant111/glowdeck-sports/actions/runs/41480616401
- ESP32 portable tests **34/34 PASS** and CYD firmware compilation: **PASS** — https://github.com/ggrant111/glowdeck-sports/actions/runs/41480616371
- Cross-platform parity: **PASS** — https://github.com/ggrant111/glowdeck-sports/actions/runs/41480616151
- APK SHA-256: `79c79984a265a1bf97406b3fbbcfd10993b481c1b1272871afbc4e1d981e1ec9`.
- APK artifact: https://github.com/ggrant111/glowdeck-sports/actions/runs/41480616401/artifacts/13160245573
- ESP32 firmware artifact: https://github.com/ggrant111/glowdeck-sports/actions/runs/41480616371/artifacts/13159927195

The ESP32 now also keeps the final-goal window when only goal effects are enabled, matching Fire TV. Native regression coverage includes this configuration. All seven effect patterns, team filters, toggles, previews, delay and journaled restoration are implemented on both physical platforms. Browser preview remains a mock and the separate hosted deployment is unchanged.

These are successful software builds, not hardware acceptance. Verify WLED mapping/restoration, CYD speaker/touch/heap behavior and Fire TV playback performance before considering the feature production-ready. The branch has not been merged into main.


## v0.5.2 — upcoming games and controller usability

Implemented device paging (ESP32, capacity 8), upcoming-game waiting/arming,
seven-day date browsing, local kickoff display, game-ID monitoring across date
changes, explicit optional reboot resume, and local browser mock coverage.
ESP32/Fire TV retain event masks, team mode, delay, no-replay baselines and scene
restoration. See `docs/V052_FIELD_TEST.md` for scope and hardware acceptance.
Automated validation is recorded with the release builds; real-device acceptance
is pending. Hosted Lovable simulator remains a separate deployment.

## v0.6 multi-game contract

ESP32 and Fire TV now support four followed games, with independent display/sound settings and A/B/both/none routing to two WLED targets. Each game owns its baseline, delay and cooldown; each target owns its priority queue and original-state restoration journal. Queues hold eight events, expire waiting events after 20 seconds, and never interrupt the current eight-second reaction. Failed restoration blocks further effects on that target until recovery succeeds. Saved preferences do not re-arm multi-game monitoring after reboot.

ESP32 detailed settings are on its existing local web page, with follow/start/stop/status on the touchscreen. Fire TV exposes these controls under GAME. Sound is queued independently of busy lighting targets. See [setup, exact rules, and hardware acceptance checks](V0.6-FOLLOWING.md). Browser simulator remains a single-game demonstration; its current effect regression tests still run, but it does not model v0.6 multi-game routing. The paged web redesign and full-screen carousel are subsequent releases.
