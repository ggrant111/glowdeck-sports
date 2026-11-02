# GlowDeck GameDay v0.2 — Sprint 2 (hardware testing pending)

**ESP32-2432S028R (2.8" CYD) • 320×240 landscape touchscreen • PlatformIO**

Tracking: [PROJECT_PLAN.md](PROJECT_PLAN.md) · [Progress log](docs/PROGRESS_LOG.md) · [Sprint 2 design](docs/SPRINT2.md) · [Validation](docs/VALIDATION.md)

**NEW: [GlowDeck GameDay Fire TV APK](firetv/README.md)** — a native, D-pad-controlled Fire TV test console. Select real NHL games, inspect play-by-play, pair a local WLED IP, preview team colors with restore, and optionally arm safe new-goal reactions. [Download APK](https://github.com/ggrant111/glowdeck-sports/releases/tag/firetv-v0.1.0-test). This is a **separate test platform**; it does not replace the ESP32 firmware.

## Try v0.2 without the ESP32

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

GitHub Actions → **Build and test GameDay firmware** automatically runs the native tests, builds the ESP32 firmware, and publishes `GlowDeck-GameDay-v0.2-firmware` artifact. A single `firmware.bin` should **not** be flashed at offset 0: use PlatformIO Upload so bootloader and partitions are handled correctly.

## Project structure

- `src/GameCore.cpp` — NHL model and static team-color mapping
- `src/GoalEngine.cpp` — platform-neutral goal detection, delay, cooldown, recovery scheduling
- `src/WledClient.cpp` — local WLED JSON API + NVS crash-recovery journal
- `src/main.cpp` — ESP32 Wi-Fi, web server, app lifecycle, automatic and manual safety gates
- `src/Ui.cpp` — four touchscreen pages
- `src/NhlClient.cpp` — HTTP NHL scoreboard polling adapter
- `simulator/v02.cpp` — C++ replay with a fake WLED light driver
- `simulator/web/index.html` — interactive self-contained demo
- `test/test_core/` — Unity tests, native without Arduino libraries

See [PROJECT_PLAN.md](PROJECT_PLAN.md) for upcoming sprints, statuses and testing intervals.
