## ESP32 v0.8.1 — play-by-play memory fix

Fixes `Reconnecting: PBP JSON: NoMemory` after Start all. The ESP32 now processes each play separately instead of retaining the entire game history. Existing following, display and Wi-Fi settings are retained. Android already uses a different parser and does not need this firmware-only patch.

## v0.8 — game display

Both devices boot into a display cycling today’s upcoming/live games about every 10 seconds. Tap the top-left gear (Fire TV: select it with the remote) for controls. ESP32: SETUP → SHOW returns to display; Fire TV: DISPLAY tab. Scores and clock are NHL snapshots refreshed about every 60 seconds, not a locally running countdown. Logos are bundled for offline use.

## v0.7 — page navigation

Open the ESP32 local address and use Games, Following, Lights, Sound or Setup. Saves return to the current page. On Fire TV, use the new FOLLOWING tab for multi-game controls. Monitoring settings from v0.6 are retained.

## v0.6 — multiple followed games

ESP32 and Fire TV support **four followed games** and **two WLED targets**. Each game has independent device-display, goal-sound, target A/B/both/none, team-side, delay, and DDP-event settings. See [v0.6 setup and acceptance checks](docs/V0.6-FOLLOWING.md).

# GlowDeck GameDay v0.4 — Goal lights + built-in/custom audio (hardware testing pending)

**ESP32-2432S028R (2.8" CYD) • 320×240 landscape touchscreen • PlatformIO**

Tracking: [PROJECT_PLAN.md](PROJECT_PLAN.md) · [Cross-platform parity contract](docs/FEATURE_PARITY.md) · [Progress log](docs/PROGRESS_LOG.md) · [Sprint 2 design](docs/SPRINT2.md) · [Validation](docs/VALIDATION.md)

**NEW Fire TV v0.4.1: [Download LAN sound manager APK](https://github.com/ggrant111/glowdeck-sports/releases/download/firetv-v0.4.1-test/GlowDeck-GameDay-FireTV-v0.4.1.apk)** — open the Fire TV SOUND tab, then use your iPhone at `http://FIRE_TV_IP:8788/` with the pairing code shown on the TV. Upload WAVs, assign all NHL teams and preview through TV speakers. [Setup and limitations](docs/LAN_SOUND_MANAGER_V041.md). — now uses an opt-in Android foreground service, so it can continue NHL play-by-play polling and WLED goal effects while the user watches a game in another Fire TV app. Monitoring requires explicit arming and can be stopped from the app or notification. [Installation and validation](firetv/README.md). This is a **test build**; real Fire TV/Fire OS endurance tests remain open.

**NEW: [GlowDeck GameDay Fire TV APK](firetv/README.md)** — a native, D-pad-controlled Fire TV test console. Select real NHL games, inspect play-by-play, pair a local WLED IP, preview team colors with restore, and optionally arm safe new-goal reactions. [Download APK](https://github.com/ggrant111/glowdeck-sports/releases/tag/firetv-v0.1.0-test). This is a **separate test platform**; it does not replace the ESP32 firmware.

## Try v0.4 without the ESP32

**[Launch the hosted, mobile-friendly simulator](https://glowdeck-gameday-simulator.lovable.app)** — open directly in Safari or any full browser, including on iPhone. No login or hardware required. The hosted simulator uses fake NHL scores, fake WLED devices, local in-browser controls, a goal timer, and a mock event log. It does **not** drive real lights or run the ESP32 firmware. The source for the browser demo in this repository is `simulator/web/index.html`; that self-contained HTML file works in a regular JavaScript-capable browser but **may show a blank device preview when opened in iOS/ChatGPT/GitHub attachment viewers that block inline JavaScript**. Use the hosted link instead.

To test: tap **UTA @ BOS** under SCORES → GAME → **ARM GOAL LIGHTS** → **UTA goal +1** under Simulation Controls; press **Fast-forward** to watch the mock WLED color change and restoration without waiting.


This is an independently running NHL-to-WLED console; **no PC is needed once flashed**. The added v0.2 game-reaction system can be tested on a desktop before the CYD arrives. **The default at every boot is DISARMED.** Automatic scenes only run when deliberately armed and a LIVE game and WLED device are selected.

## Live NHL and real WLED testing without the touchscreen (NEW)

- **Hosted [GlowDeck Simulator](https://glowdeck-gameday-simulator.lovable.app):** choose **Demo / Live NHL**. Live scores are fetched through a restricted same-origin server proxy. All light effects in the hosted demo remain FAKE, and live games never automatically trigger lights.
- **[Local NHL + WLED test bridge](tools/LOCAL_BRIDGE.md):** run a Python 3 standard-library server on your Windows/Mac/Linux computer and open its UI from Safari on the same trusted home LAN. It reads real NHL scores and can change one explicitly configured WLED device for 2–10 seconds, with a saved scene and an automatic restore. There is no cloud relay and no automatic real-goal lighting.

Windows PowerShell, from the repository root (replace IP if your WLED has moved):

```powershell
py -3 tools\gameday_bridge.py --wled-ip 192.168.1.71 --listen 0.0.0.0
```

On iPhone open `http://<computer-WiFi-IP>:8787/`, **not** the WLED IP, and enter the bridge access code shown in the computer terminal. Run `ipconfig` on Windows to find your computer IPv4 address. Tests: `python -m unittest discover -s tools/tests -v`. This bridge is a **temporary computer-hosted test bench** and cannot validate the ESP32 hardware. Keep it off public Wi-Fi and never port-forward it.


## v0.4 sound playback — source built, physical hardware pending

**Built-in:** three original synthesized horns (Stadium Horn, Double Horn, Siren) require no microSD or internet. **Custom:** user-supplied canonical PCM mono 16000-Hz 16-bit `.wav` files up to 10 seconds from `/audio/NAME.wav` on a microSD card. The sound engine uses GPIO26 DAC / the compatible board's speaker amplifier, in a FreeRTOS worker. The SOUND touchscreen page provides mute, volume, Away/Home sound selection and independent previews; the ESP32 local IP webpage also allows entering a custom clip name.

Audio is OFF after every boot until you explicitly enable it. Audio starts from the same validated goal event, selected scoring team and TV delay as WLED, but a speaker failure never blocks WLED restoration. Invalid/missing custom WAV files fall back to the original built-in horn. A single playing custom clip is stopped if lights restore or you disarm the monitor.

**Board-specific limitation:** the microSD card and resistive touchscreen use the same ESP32 VSPI hardware host on separate GPIO assignments. v0.4 temporarily pauses touch polling while a custom file streams, then reinitializes touch. NHL and lighting tasks remain active, but physical touch responsiveness during custom WAV playback is **not yet proven**. Confirm GPIO26, amplifier and speaker connector on your board revision before wiring or enabling sound.

To generate original sample WAV files with no external dependencies, run `python tools/make_test_wavs.py ./audio` and copy the generated files into `/audio/` on the SD card. Example audio names in the ESP32 web interface: `HORN1`, `HORN2`, `SIREN` (uppercase, no extension). See [complete sound setup/known limitations](docs/AUDIO_V04.md).

**Final `main` CI:** [28/28 native tests + CYD firmware](https://github.com/ggrant111/glowdeck-sports/actions/runs/41383412590), [Fire TV JVM tests and APK](https://github.com/ggrant111/glowdeck-sports/actions/runs/41383412450), [cross-platform parity](https://github.com/ggrant111/glowdeck-sports/actions/runs/41383412448). Physical speaker, touch, streaming audio-focus and microSD tests remain open.

## ESP32 v0.3: matching Fire TV v0.3 gameplay

The four-screen 320x240 touchscreen now supports **AWAY**, **HOME** and **BOTH** NHL goal celebrations. Each game remembers its celebration mode in NVS. The new goal feed uses NHL **play-by-play event IDs** and server-provided goal owner team IDs rather than scoreboard-only deltas. It polls play-by-play approximately every 25 seconds while explicitly armed, captures a fresh baseline on arming or after feed recovery, ignores ambiguous multi-goal batches, and restores the prior WLED segment-0 scene after **eight seconds**. Modes are also available on the ESP32's local setup webpage. Default after reboot is DISARMED.

This brings software behavior closer to the **Fire TV v0.3**, including scoring team colors, 0-90-second TV delay, 20-second cooldown, network fail-closed behavior, and WLED state journaling. The ESP32 is standalone; it does not require the Fire TV app. **Never arm both controllers against the same WLED controller** until cross-controller coordination is implemented.

**Build/test (PlatformIO):** `pio test -e native` and `pio run -e cyd`. Github Actions builds a `GlowDeck-GameDay-v0.4-firmware` artifact; upload with PlatformIO rather than flashing that single BIN at offset 0.

**Hardware blockers:** ESP32 touchscreen and WLED behavior have not been tested on a real board, and the prototype still skips NHL TLS certificate verification. Check actual touch mapping, Wi-Fi, NHL PBP JSON parse memory, and real WLED scene restoration on board before trusting unattended operation.

## What changed in v0.2

- Four touchscreen tabs: **SCORES**, **GAME**, **LIGHTS**, **SETUP**. GAME shows the selected live matchup, AWAY/HOME favorite-team choice, 0–90-second broadcast delay (15-second on-device steps), and ARM/DISARM action.
- A portable `GoalEngine` conservatively reacts to an **unambiguous single-score increase** while already armed on a live game. No initial-sync goals, no replays after reboot; duplicates and ambiguous score jumps are suppressed. Corrections, multigoal jumps, selected-game changes, missed-feed expiry, and Wi-Fi outage disarm.
- Automatic reactions are **10-second SOLID team-color scenes** (no strobe). A 20-second cooldown prevents overlapping scenes. Stage a scene only if a WLED segment-0 state snapshot is safely written to ESP32 NVS.
- Snapshot restores **segment 0** and master on/brightness after timeout, disarm, Wi-Fi recovery, or device restart. It preserves supported effect/color/palette/speed/intensity fields. If restoration fails, the recovery record is retained and retried when Wi-Fi returns. New automatic scenes are blocked until restoration succeeds.
- Additional local web controls: select game, select favorite home/away, adjust delay, arm/disarm. Existing manual WLED and NHL setup remain available.
- Hardware-free native goal-event simulator, 17 Unity cases including restart/retry/delay edge cases, plus a standalone [browser UI simulator](simulator/web/index.html) for previewing all four pages. The HTML simulator is a separate mock, **not firmware or a real NHL/WLED connection**.

### Important limitations

- NHL scoreboard data does **not** expose authoritative goal/play IDs. This version detects single-score deltas, not confirmed NHL play-by-play events; rare corrections/late-feed cases may be ambiguous. It fails closed on detected ambiguous cases. Event-ID confirmation is planned for v0.3. **Only use automatic lighting on nonessential WLED segment 0 until physical validation.**
- Auto effects require a successful fresh NHL poll and one selected WLED. GAME arm is rejected for future/finished games. The selected game is not restored across reboot, nor is armed status. Favorite side and delay are persisted.
- WLED original state is captured for segment 0 and the top-level on/brightness state, not the full controller configuration. A controller changed externally during a reaction may be overwritten by restoration. Multi-segment zones are a future sprint.
- Prototype NHL TLS uses `setInsecure()`; local HTTP setup has **no login**. Do not expose the ESP32's port 80 to the internet. TLS validation and setup hardening remain release blockers.
- CYD hardware hasn't been tested. Display pins, touch calibration, Wi-Fi captive portal, live NHL requests and physical WLED restore still require on-device acceptance. A successful CI compilation is not hardware proof.

## Build and flash

1. Install **Visual Studio Code** + **PlatformIO IDE**.
2. Clone this repository or download ZIP; open the folder containing `platformio.ini`.
3. Connect **ESP32-2432S028R** by a USB data cable.
4. Run `pio run -e cyd` and `pio run -e cyd -t upload` (or PlatformIO → cyd → Build / Upload).
5. Watch the serial log with `pio device monitor -b 115200`. If uploading fails at 460800, lower `upload_speed` to 115200.
6. Join the `GlowDeck-GameDay` setup Wi-Fi if no saved credentials; configure your 2.4GHz Wi-Fi in the captive portal or `http://192.168.4.1`.
7. On SETUP note the local IP, and open `http://<device-ip>/` on the **same trusted LAN**. Add WLED by IP (e.g. a controller presently using `192.168.1.71`), or use scan. On SCORES tap a game, on LIGHTS select a device, on GAME choose a favorite side and delay. **Arm only after confirming the selected game is LIVE.**
8. GAME arm/disarm is a safety switch. When you disarm, an active automatic scene restores the original WLED state. Manual AWAY/HOME/OFF actions also require successful restoration first.

**Touch pinout:** separate XPT2046 SPI: CLK25, MISO39, MOSI32, CS33, IRQ36; ILI9341 via TFT_eSPI (CLK14, MOSI13, MISO12, CS15, DC2), backlight21. Approximate raw touch limits are in `src/main.cpp`; calibration is required per board revision. Hold BOOT at power-on to clear saved Wi-Fi configuration only.

## Test without the touchscreen

Open `simulator/web/index.html` in a desktop or mobile browser. It runs a local mock of the four tabs with simulated scoring, device controls, arm/disarm, delay and restart. No API key or webserver needed.

On any desktop with C++17:

```bash
./scripts/check-native.sh
```

With PlatformIO installed, execute the same automated test suite used in GitHub Actions:

```bash
pio test -e native
pio run -e cyd
```

GitHub Actions → **Build and test GameDay firmware** automatically runs the native tests, builds the ESP32 firmware, and publishes `GlowDeck-GameDay-v0.4-firmware` artifact. A single `firmware.bin` should **not** be flashed at offset 0: use PlatformIO Upload so bootloader and partitions are handled correctly.

## Project structure

- `src/GameCore.cpp` — NHL model and static team-color mapping
- `src/GoalEngine.cpp` — platform-neutral goal detection, delay, cooldown, recovery scheduling
- `src/WledClient.cpp` — local WLED JSON API + NVS crash-recovery journal
- `src/main.cpp` — ESP32 Wi-Fi, web server, app lifecycle, automatic and manual safety gates
- `src/Ui.cpp` — five touchscreen pages including SOUND
- `src/NhlClient.cpp` — HTTP NHL scoreboard and play-by-play polling adapter
- `simulator/v02.cpp` — v0.3 event-ID C++ replay with a fake WLED light driver
- `simulator/web/index.html` — interactive self-contained demo
- `test/test_core/` — Unity tests, native without Arduino libraries

See [PROJECT_PLAN.md](PROJECT_PLAN.md) for upcoming sprints, statuses and testing intervals.
