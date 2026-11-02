# GameDay v0.5.0 — DDP event effects

Implemented source; local firmware build and Android source compilation passed. APK packaging and physical acceptance remain pending. DDP is opt-in; upgrading keeps solid-color goal mode until enabled. Fire TV, ESP32 and the checked-in browser mock have the same seven event patterns. The hosted Lovable deployment is separate and has not been updated by this change.

## First release behavior

| Event | Effect | Automatic trigger |
|---|---|---|
| Goal | Team-color burst / chase | Existing unique PBP goal IDs; shootout attempts excluded |
| Power play | Moving chase | First PBP play confirming a change to unequal skater counts with both goalies present |
| Period start | Filling sweep | `period-start` with regular-period descriptor |
| Period end | Fade down | `period-end` |
| Game end | Moving finale | New `game-end` or live-to-final PBP state transition, once only |
| Overtime | Slow pulse | `period-start` with `periodType=OT` |
| Shootout | Alternating bands | `period-start` with `periodType=SO` |

Each cue lasts eight seconds. Power-play lighting is a short notification, not a continuous representation of the penalty clock. A penalty alone does not trigger it, and empty-net advantages are excluded. Detection follows the published feed and may lag the actual whistle. A game must be LIVE when armed; the first-period opening cue requires arming before that event appears, so it may be missed if the feed has already published it.

AWAY/HOME/BOTH filters goals and power plays. Neutral phase cues apply in all three modes and use the selected side's primary color; BOTH uses away color for neutral cues. Goal sound behavior is unchanged. Other event sounds are not introduced in this release.

All events honor the 0–90-second TV delay. Historical baseline, outage recovery and deduplication suppress replay. One cue is scheduled at a time; highest priority in a poll is goal, game end, shootout, OT, period start, power play, period end. Busy/cooldown drops lower-priority cues. Game end is retained behind a goal until the next eligible poll. Goal reactions retain the existing ambiguity safeguard. A final-state session ends after its queued cues and grace period, rather than stopping before the game-end scene.

## Setup and controls

- Fire TV: LIGHTS → ENABLE DDP WHOLE STRIP. Each event has ON/OFF and Preview Away/Home buttons. Stop monitoring before editing or previewing. Select a LIVE game and arm as before.
- ESP32: SETUP → EFFECTS. Toggle DDP, cycle event, enable/disable it, and preview either side. The local setup page offers the same controls.
- Browser: open `simulator/web/index.html` with `effects.js` beside it. Controls preview all seven patterns on a mock strip; simulated events honor mode/delay/arming. It does not send UDP or run the production PBP parser.
- WLED: realtime receive enabled; Main segment only disabled for this whole-device mode; live override off. Use a finite realtime timeout (for example 2.5 seconds). Keep just one GameDay controller/other realtime sender on a target. Existing active realtime receivers are refused at scene start.

## Transport and restoration

Native Java DatagramSocket and ESP32 WiFiUDP send unicast DDP v1 to UDP **4048**. RGB24 type 0x0B, destination 1, byte offsets, 480-pixel chunks, final-chunk PUSH, sequence 1–15. Maximum 600 pixels on one selected device; oversized targets are refused rather than truncated. This is whole-device output, not isolated segment streaming. No multi-device scheduler is claimed.

Dedicated workers cap output at 20fps, skip catch-up bursts, and stop independently at eight seconds. Pixel channels are capped at 160 even if WLED forces realtime brightness; the master brightness request is also capped at 160. RGBW white-channel mixing remains WLED-controlled. A 600-pixel target uses roughly 36.4 kB/s in DDP payload/header bytes at 20fps, plus network overhead.

Persist the original state before sending, stop UDP before restoration, send `live:false`, restore prior power/brightness/segment-0 settings, then clear the journal only after HTTP success. DDP does not modify other segments' saved effect settings; realtime release lets those effects resume. The existing journal survives restart. Failed restores block new scenes. Network request scheduling can delay the explicit restore beyond eight seconds; the stream itself is bounded and WLED's finite realtime timeout provides receiver-side release.

DDP packets are best effort: send success does not confirm displayed pixels. This implementation cannot prevent another controller starting after its initial live check. WLED realtime configuration, transitions, RGBW mapping, packet loss and hardware memory behavior require the tests below.

## Acceptance

1. Preview every effect on Fireplace; verify whole-strip mapping, original ON and OFF restoration, brightness and previous animated effects.
2. Turn off DDP; verify existing solid-color goal mode and custom sound upload still work.
3. Enable only one event at a time, then verify AWAY/HOME/BOTH, TV delay, duplicates, multiple simultaneous events and final goal followed by game end.
4. Test STOP, leaving the Fire TV UI during a preview, Wi-Fi loss, restart during a stream, failed HTTP restoration and journal recovery.
5. Test 1, 480, 481 and 600 LEDs; reject 601. Verify packet offsets, final PUSH and no packets after cancellation/restoration.
6. Run Fire TV alongside YouTube TV; check video smoothness and audio. On CYD test simultaneous WAV playback, touch, 72-KiB PBP parsing and DDP worker under full-game load.
7. Run the existing 24-hour soak and three live-game release gates. NHL polling remains 25 seconds; realtime animation does not make the upstream sports feed realtime.

References: https://kno.wled.ge/interfaces/ddp/ and https://kno.wled.ge/interfaces/json-api/ .


### Local validation results (2026-11-02)

- ESP32 `pio test -e native`: **33/33 passed**.
- ESP32 `pio run -e cyd`: **passed**, firmware produced; static RAM 51,284 bytes (15.7%), flash 1,211,741 bytes (92.4%). Heap under live NHL + audio + DDP still needs board measurement.
- Java portable production classes and **39 JVM unit tests passed**, using JUnit 4.13.2 and org.json. All Fire TV production Java sources also compiled against Android API 35.
- DDP socket integration test is present but **skipped locally** because no suitable private interface was available; no real WLED UDP delivery is claimed.
- **175 C++/browser pixel vectors**, browser event-handler tests with deterministic timers, static parity checks and **9 bridge regression tests passed**.
- Browser visual/mobile test could not run: Chromium download was unavailable. Hosted simulator remains unchanged.
- Full Gradle APK packaging/Android CI and physical tests remain pending. The user authorized the GitHub branch push and APK build on 2026-11-02; remote build results are recorded in the follow-up entry.
