# GlowDeck GameDay — Fire TV Lab v0.1

A **native Fire TV / Android TV test application** for testing the public NHL API and real local WLED devices *before* the ESP32-2432S028R touchscreen arrives. No Lovable bridge, WebView, PC server or account is needed once the APK is sideloaded. Compatible with Fire OS/Android APIs 22+; D-pad SELECT navigation with a Leanback launcher and 320x180 TV banner.

## Download the APK

**[Download Fire TV test APK](https://github.com/ggrant111/glowdeck-sports/releases/download/firetv-v0.1.0-test/GlowDeck-GameDay-FireTV-v0.1.apk)** from the [verified prerelease](https://github.com/ggrant111/glowdeck-sports/releases/tag/firetv-v0.1.0-test). Download the `.apk`, not the source ZIP. Build/tests: [GitHub Actions run 41372267341](https://github.com/ggrant111/glowdeck-sports/actions/runs/41372267341). Alternatively visit **GitHub Actions → Fire TV test APK → most recent successful run → Artifacts**.

Build from source (Android Studio): open this `firetv/` directory as a standalone Android project. Requires Android SDK 35, JDK 17, Android Gradle Plugin 8.5.2. Use `gradle :app:assembleDebug`; the APK is at `app/build/outputs/apk/debug/app-debug.apk`. Native tests `gradle :app:testDebugUnitTest`.

## Sideload to your Fire TV

1. On the Fire TV, enable **Developer Options** and **ADB debugging** (and permit installing unknown apps where necessary). Put the Fire TV on the same home Wi-Fi as your PC and WLED device.
2. Find its IP in **Settings → My Fire TV → About → Network**.
3. From the PC with Android platform-tools installed:

   ```powershell
   adb connect 192.168.1.24
   adb install -r GlowDeck-GameDay-FireTV-v0.1.apk
   adb shell am start -n com.gkowdeck.gameday.firetv/.MainActivity
   ```

   **Replace `192.168.1.24` with your current Fire TV IP**; it was only a previously used example. If a debug-signature mismatch reports `INSTALL_FAILED_UPDATE_INCOMPATIBLE`, remove the previous lab build first via `adb uninstall com.gkowdeck.gameday.firetv` (you will lose saved device IP/delay settings), then install again. This is test signing, not production signing.
4. Open **GlowDeck GameDay** on your Apps screen, or launch using the last ADB command.

## On-TV test procedure

1. **NHL SCORES** → press **REFRESH NHL**. Expect today's slate and live/final/upcoming statuses. A no-game day is valid. If an error appears, open **LOG / HELP** for HTTP/SSL details.
2. Select an NHL matchup using the D-pad and center Select; this opens **GAME / EVENTS**. Press **FETCH GAME EVENTS** and inspect numbered NHL play-by-play events. For a game that hasn't started, plays may be empty.
3. Open **WLED LIGHTS**, enter the device's IPv4 address (for example the known Fireplace at `192.168.1.71`, **if unchanged**), then **SAVE IP + CONNECT**. The app reads `GET http://<ip>/json/info` and confirms its WLED version/name.
4. Press **READ WLED STATE**; verify on/brightness/segment count in LOG. Then press **PREVIEW AWAY COLOR (8s)** or **PREVIEW HOME COLOR (8s)**. The app saves selected WLED master + segment-0 state locally, applies a *solid* team color at capped brightness 160, and restores that state after 8 seconds. Use **RESTORE PREVIOUS SCENE** to restore immediately. Do not test on critical lighting or automatic safety equipment.
5. **Optional live goal test:** Select a LIVE NHL matchup, confirm WLED connected, choose AWAY/HOME favorite side and TV delay 0–90s, then press **ARM LIVE GOALS**. The app first loads existing goal IDs from NHL play-by-play and ignores those old goals. While visible it polls for new play events approximately every 25 seconds. A unique new goal by your chosen team triggers a single solid 8-second team-color preview (after configured delay), then restores the old scene. **No arming survives restart/backgrounding**, no strobe, no automatic action until you arm. Multiple unseen goals in one poll are suppressed; if NHL ownership is missing, no trigger.

## Safety and limitations

- No physical Fire TV/WLED hardware tests have been run by the assistant. Android compilation and local JVM tests can be verified by the GitHub Actions workflow; hardware/actual network behavior remains to be verified in your LAN.
- The NHL API (`https://api-web.nhle.com/v1/score/now`, `/v1/gamecenter/{id}/play-by-play`) is a public, best-effort, undocumented service. NHL network availability, payload fields and live event latency can change without notice.
- WLED communication is **unencrypted local HTTP** (`/json/info`, `/json/state`). The app accepts only private 10.x, 172.16–31.x, 192.168.x IPv4 addresses and never accesses arbitrary hosts, public IPs, or cloud bridges. It needs internet for NHL and LAN access for WLED.
- The selected WLED is one manually entered device, **segment 0 only**. Master brightness is temporarily affected and restored. State restoration is journaled for recovery after a crash. WLED changed by another controller during the 8-second preview may be overwritten by the restoration; test carefully. If restoration fails, the previous journal is retained; reopen the app and use RESTORE before testing further.
- A timer or network outage can delay a restoration; avoid using this tool for time-critical/safety-critical lighting. For a completely unresponsive WLED, use its own interface to restore lighting.
- App fetches today's NHL scoreboard once per minute in foreground; when armed and a live game is selected, selected play-by-play once per ~25 seconds. API freshness will vary; watching a broadcast or streaming provider can have additional delays.
- Pressing HOME or leaving app disarms monitoring. It is a diagnostic lab, not a guaranteed unattended live scoring automation.
- Downloaded APK is a **debug-signed test build**. Do not rely on this signing key for future production installation or auto-updates.

## Source and project tracking

Source: `app/src/main/java/com/gkowdeck/gameday/firetv/` and corresponding tests. Firmware ESP32 project stays at repository root and can evolve independently. Add hardware acceptance outcomes to `docs/VALIDATION.md` and status to `PROJECT_PLAN.md`.
