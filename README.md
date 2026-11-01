# GlowDeck GameDay v0.1 — Sprint 1

**Project roadmap and sprint tracking:** [PROJECT_PLAN.md](PROJECT_PLAN.md) · **Progress history:** [docs/PROGRESS_LOG.md](docs/PROGRESS_LOG.md) · **Evidence:** [docs/VALIDATION.md](docs/VALIDATION.md)

**Target:** ESP32-2432S028R *2.8-inch ILI9341 resistive-touch CYD*, classic ESP32, 4 MB flash. This is a **source release for PlatformIO**, not a preflashed device. No external server, computer, or API key required after initial setup.

## Features implemented

- 320×240 landscape color touchscreen (TFT_eSPI + XPT2046 on **separate SPI buses**).
- On-device Wi-Fi setup portal named **GlowDeck-GameDay** on first boot.
- Daily NHL scoreboard from `https://api-web.nhle.com/v1/score/now`; refresh every 60 seconds when connected.
- Select a game, view scores, monitor score changes without automatically controlling lights.
- WLED discovery through `_wled._tcp` mDNS, local HTTP validation and persisted IP list.
- Local web setup UI at `http://<device-ip>/` to add/remove WLED by IP (useful if mDNS discovery is blocked).
- Tap **AWAY** or **HOME** to send that team's primary color to the selected WLED controller (segment 0), or **OFF** to switch it off.
- Portable core `GameCore.cpp`, simulated match timeline, 7 Unity tests, GitHub Actions workflow.

**Deliberately excluded until Sprint 2:** autonomous goal-triggered LED effects, complex zones/segments, play-by-play event detection, third-party sports, OTA firmware, and production authentication. The NHL endpoint is public but not a contractual supported SDK.

## 1. Build and upload (Windows/macOS/Linux)

1. Install [VS Code](https://code.visualstudio.com/) and the **PlatformIO IDE** extension.
2. Extract this ZIP and open the top-level `GlowDeck-GameDay-v0.1` folder in VS Code (the folder that has `platformio.ini`).
3. Plug the CYD ESP32 into USB. A **USB-A to USB-C cable** may be required on certain boards; some CYD USB-C ports do not negotiate USB-C-to-USB-C power.
4. Click **PlatformIO → Project Tasks → cyd → General → Build**, then **Upload**. The firmware build outputs `.pio/build/cyd/firmware.bin`.
5. Alternatively, run:
   ```bash
   pio run -e cyd
   pio run -e cyd -t upload
   pio device monitor -b 115200
   ```
6. First start: if the board has no saved Wi-Fi credentials, connect your phone to the `GlowDeck-GameDay` Wi-Fi AP. The WiFiManager portal should open automatically. If not, visit `http://192.168.4.1/`. Choose your home 2.4 GHz Wi-Fi and enter its password. The ESP32 then joins your network.
7. On the **SETUP** screen, note the device IP, visit `http://<device-ip>/` from a phone on the same Wi-Fi network, and add WLED controllers by their IPv4 addresses. Use **SCAN** on the Lights screen for mDNS discovery.
8. On **SCORES**, tap a game. On **LIGHTS**, tap a WLED device, then **AWAY** or **HOME**. Lights should change to that team's color.

### Flash troubleshooting

- If flashing fails, lower `upload_speed` from 460800 to 115200. Hold **BOOT**, tap reset, then release BOOT as uploading begins if the board does not auto-enter bootloader.
- Confirm *your board* is an `ESP32-2432S028R` with ILI9341 and XPT2046. CYD lookalikes may use ST7789 or different pins.
- If the screen stays blank, check the board's backlight GPIO21 and the ILI9341 driver in `platformio.ini`.
- If touch is inverted or offset, follow the touch-calibration checklist below before changing the logic.
- To clear saved Wi-Fi credentials, **hold the BOOT button when powering on**; saved WLED devices are retained. The 3-minute captive portal then opens.
- Check Serial Monitor at 115200 for NHL HTTP status, JSON errors and raw touch coordinates.

## 2. Touch calibration

Touch uses its own SPI bus: SCK 25, MOSI 32, MISO 39, CS 33, IRQ 36. The initial raw-to-screen calibration is intentionally approximate. In `src/main.cpp`, adjust `RAW_X_MIN`, `RAW_X_MAX`, `RAW_Y_MIN`, `RAW_Y_MAX`, and `touch.setRotation(1)` to match the four corners of your board. The serial log shows both raw and mapped coordinates for each tap. **Record actual min/max values from each edge before changing these constants.** A mirrored panel may require swapping or reversing axes; modify only `pollTouch()` mapping for that board.

## 3. WLED behavior and safe acceptance tests

- WLED APIs: `GET /json/info`, `POST /json/state`. No lighting is triggered automatically by NHL updates in this sprint.
- Tap HOME/AWAY changes **WLED segment 0**, sets the Solid effect and brightness 160, and turns it on. It does **not** preserve a previous effect/brightness state. Run your test on a nonessential light/segment first.
- Only plain IPv4 LAN addresses are accepted by the web configuration form. Up to **8** WLED controllers are persisted in ESP32 NVS.
- First try adding your known WLED device directly, such as `192.168.1.71`, if still assigned to your Fireplace.
- Discovery may not find WLED devices across VLANs, guest Wi-Fi networks, or access points that suppress multicast.
- This v0.1 HTTP setup interface has no login or TLS. Keep it on a **trusted private Wi-Fi LAN**; do not expose port 80 to the internet.
- NHL HTTPS is encrypted but uses `setInsecure()` **without certificate verification** in this proof of concept. Do not send secrets via this connection. Certificate validation is required before production use.

## 4. Offline simulation and automated tests

Run from the project directory:

```bash
./scripts/check-native.sh
# or, if PlatformIO is installed:
pio test -e native
```

The standalone simulator compiled with your desktop C++ compiler produces a deterministic UTA–BOS timeline. It reports two **display-only** goal alerts and **zero** automatic WLED actions. Hardware networking and the TFT are intentionally not part of the desktop simulator.

### Automated build / artifact

Push the source to GitHub and open **Actions → Build and test GameDay firmware**. The workflow runs the native tests, compiles the ESP32 target and publishes `firmware.bin` as a build artifact. A **bare `firmware.bin` alone is not guaranteed to flash correctly at offset 0**; prefer the PlatformIO Upload action, which flashes bootloader, partitions and firmware at their respective offsets.

## 5. Sprint 1 acceptance checklist

| Check | Pass condition |
|---|---|
| Firmware compile | `pio run -e cyd` exits 0 |
| Offline simulation | 2 display-only alerts, 0 automatic WLED sends |
| Touch | Four corners and three tabs respond consistently |
| Wi-Fi | Connects via captive portal, IP shown in SETUP |
| NHL | Current-date scoreboard loads or a useful error is shown |
| WLED | Discovery **or** direct-IP pairing works |
| Manual color | HOME/AWAY updates chosen controller's segment 0 |
| No auto effects | Score update NEVER sends a WLED request |
| Power-cycle | Wi-Fi and WLED IP list retained |

**Verification status (2026-11-01):** portable simulator passed locally. GitHub Actions [run 41364609955](https://github.com/ggrant111/glowdeck-sports/actions/runs/41364609955) passed all 7 native Unity tests, built the ESP32 target and uploaded the firmware artifact. **Hardware flashing, touchscreen calibration, live NHL polling and real WLED control are still unverified.** Hardware requires your ESP32 unit. See `docs/VALIDATION.md` for exact evidence and known issues.

## Project tree

```text
platformio.ini
include/GameCore.h         # portable domain model + color map
include/NhlClient.h        # NHL network adapter API
include/WledClient.h       # WLED network/discovery/persistence API
include/Ui.h               # touchscreen views and touch actions
src/GameCore.cpp
src/NhlClient.cpp
src/WledClient.cpp
src/Ui.cpp
src/main.cpp              # ESP32 hardware, local web setup, app loop
simulator/main.cpp        # offline deterministic desktop simulation
test/test_core/test_main.cpp  # Unity tests for portable core
scripts/check-native.sh
.github/workflows/build.yml
```

## References

- CYD hardware: https://randomnerdtutorials.com/esp32-cheap-yellow-display-cyd-pinout-esp32-2432s028r/
- WLED JSON API: https://github.com/wled/WLED-Docs/blob/main/docs/interfaces/json-api.md
- NHL community endpoint notes: https://github.com/pseudo-r/Public-NHL-API/blob/main/docs/web-api/games.md
