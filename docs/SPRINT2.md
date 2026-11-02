# Sprint 2 — v0.2 automatic goal engine

**Status:** v0.2 firmware source and 17/17 native tests **PASSED GitHub Actions CI** ([run 41368433335](https://github.com/ggrant111/glowdeck-sports/actions/runs/41368433335)); physical CYD hardware acceptance pending.

## Behavioral contract

1. After boot, all automatic lighting remains **DISARMED**, even if favorite team and delay are saved. Selecting a different game also disarms.
2. Select a game in SCORES, a controller in LIGHTS, and favorite HOME/AWAY side in GAME. Set 0–90-second TV delay using 15-second buttons; only an actually LIVE and freshly polled game can be armed.
3. The current score becomes the initial baseline. A subsequent **single-goal delta** while the same game is LIVE schedules one event. Duplicated identical score frames do nothing; multi-goal jumps, score corrections, final-state changes, feed expiry and Wi-Fi loss disarm or cancel. Score-delta detection is deliberately not treated as NHL event-ID verification.
4. Only one queued event / one lighting scene can run; reactions have 20-second cooldown. Pending events use 32-bit wraparound-safe timers; stale events are dropped. A late goal during an active scene is suppressed, not queued.
5. Immediately before an automatic color, GET WLED `/json/state`, save limited mutable state for **segment 0** and global on/brightness into one ESP32 NVS JSON record with the WLED IP, then POST a static team color at brightness 160. Failure to obtain and persist the snapshot aborts the change.
6. After ten seconds, or earlier if disarmed/offline, replay the saved state to the snapshotted device. If POST restore fails, retain the NVS journal and retry on Wi-Fi recovery, never overwrite it with a new automatic scene. A board reboot recovers a saved journal without automatically rearming.
7. Manual HOME/AWAY/OFF and game/device selection must restore an active automatic scene first. If restore fails, refuse manual commands rather than losing the original state.

## Test matrix — hardware-free first

| Scenario | Expected |
|---|---|
| Default / reboot | Disabled; no automatic sends |
| Arming FUT/PRE/FINAL | Rejected |
| First live baseline 1–0 | No event |
| LIVE 1–0 → 2–0 | One queued goal, favorite or opponent team color |
| Repeated 2–0 poll | No duplicate |
| +2 missing-poll jump | Cancel and disarm |
| Downward correction | Cancel and disarm |
| Toggle arm off before due | Never start a WLED scene |
| 0 / 15 / 90-second delay | Fire no earlier than due, reject expired/stale pending |
| Goal during cooldown | Suppress extra scene |
| WLED snapshot fails | Zero light writes |
| Successful scene | Backup then color then restoration |
| WLED restore fails | Keep journal, retry on reconnect |
| ESP32 restart mid-scene | On reconnect restore; remain disarmed |
| Wi-Fi/NHL disconnect | Disarm and cancel pending; recover saved scene |
| Manual color while auto active | Restore original before manual override |
| 32-bit millis wraparound | Timer calculations remain correct |

## On-device acceptance (blocked until the touchscreen arrives)

- [ ] Flash ESP32-2432S028R and calibrate display/touch.
- [ ] Verify all four tabs and HOME/AWAY favorite controls.
- [ ] Confirm saved favorite-side and delay settings survive power loss, but arming does not.
- [ ] Connect an isolated/noncritical WLED controller and test a single synthetic goal scene with safe brightness.
- [ ] Disconnect Wi-Fi mid-scene; verify journal retention and eventual restore.
- [ ] Power-cycle ESP32 mid-scene; verify NVS recovery and that no new goal auto scene starts.
- [ ] Compare real NHL score changes to broadcast delay; verify no false triggers.
- [ ] Confirm no unintended cross-segment or WLED UDP sync side effects.

**Risks:** A score delta is not an event-ID guarantee. Physical WLED segment variations and NVS restore behavior are unverified. TLS identity validation and authenticated configuration are postponed release blockers.

**Follow-on v0.3:** Consume NHL play-by-play goal `eventId`s, validate game status and integrate power plays, intermission and final-result event policies.
