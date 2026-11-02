# GlowDeck GameDay Fire TV v0.4.1 — background goal monitoring + local sound management

**A native, remote-controlled Amazon Fire TV / Android TV NHL + WLED test application.** This release introduces an opt-in Android **foreground service** that continues NHL play-by-play polling and controlling local WLED lights while the Fire TV displays YouTube TV, ESPN, Prime Video, or another app. No desktop server and no ESP32 are needed once the APK is installed.

**[Download Fire TV v0.4.1 test APK](https://github.com/ggrant111/glowdeck-sports/releases/tag/firetv-v0.4.1-test)** · [Release page](https://github.com/ggrant111/glowdeck-sports/releases/tag/firetv-v0.4.0-test) · [Project roadmap](../PROJECT_PLAN.md)

## Install by ADB

1. Enable ADB debugging on Fire TV. Put the Fire TV and WLED on the same reachable trusted network.
2. Download `GlowDeck-GameDay-FireTV-v0.4.apk` to your computer. On Windows with ADB:

   ```powershell
   adb connect 192.168.1.24
   adb install -r GlowDeck-GameDay-FireTV-v0.4.apk
   adb shell am start -n com.gkowdeck.gameday.firetv/.MainActivity
   ```

   Replace `192.168.1.24` with **your Fire TV's current IP**. The app package remains `com.gkowdeck.gameday.firetv`. Each game's celebration mode is saved separately. Default is AWAY for new games; change to BOTH for both teams. Because GitHub builds use an ephemeral debug signing key, an update can fail `INSTALL_FAILED_UPDATE_INCOMPATIBLE`. In that case use `adb uninstall com.gkowdeck.gameday.firetv` **and then reinstall**; this clears saved WLED settings.

## Test in order

1. **WLED LIGHTS** → enter a private LAN WLED IP. The previously used Fireplace IP was `192.168.1.71` (check that it hasn't changed). Select **SAVE IP + CONNECT**. Read WLED state, and test eight-second UTA blue or BOS gold previews. Confirm that your prior scene returns.
2. **NHL SCORES** → refresh. Select an **actually LIVE** game, which opens **GAME / EVENTS**. Choose **AWAY**, **HOME**, or **BOTH TEAMS** under Celebrate goals for, then set TV delay (0–90s in 15-second increments). BOTH uses the actual scoring team's NHL abbreviation/color and still restores WLED after eight seconds. This app uses NHL's public API and may need a live game before you can arm.
3. Select **ARM IN BACKGROUND**. It starts a foreground service and fetches an initial live-game and play-by-play baseline. Wait until the page says **ARMED**. Old goals are ignored.
4. Press **HOME** on the Fire TV remote and open your TV streaming app. GlowDeck continues while the foreground service is allowed to run; no picture overlay or audio capture is needed.
5. When a selected team scores (either team in BOTH mode), a single newly observed NHL goal event (event ID + team owner) schedules a WLED **solid** team-color scene after your chosen TV delay. The original segment-0/master state is journaled to disk and restored after **eight seconds**. No strobes, no TV-video scraping or microphone.
6. Return to GlowDeck's GAME / EVENTS page and select **STOP BACKGROUND MONITOR**. You can also stop through the ongoing Android notification where Fire OS exposes notifications. Confirm WLED restoration. **No automatic re-arming after a reboot, force stop, or OS process kill.**

## Functional behavior and limitations

- Foreground service (Android `dataSync`) starts only from your explicit button press. It calls `startForeground()` immediately, displays an ongoing notification with a STOP action, and returns `START_NOT_STICKY`.
- PBP polling every ~25 seconds; NHL scoreboard check ~55 seconds. Stream delay is separate from NHL data and polling latency. A unique goal from the configured team(s) starts one scene in the actual scoring team's color; older events, multi-goal batches, unknown owners, busy effects, cooldown events and corrections do not cause extra scenes.
- After network/API failure, status becomes **PAUSED**; scheduled pending goals are canceled and the resumed event batch becomes a fresh baseline. There is no replay of goals missed while offline. Monitoring stops when the selected game ceases to be live.
- Manual lighting commands, game selection, WLED IP changes and delay changes are blocked while the service is active. **Stop the monitor first.** Read-only NHL and WLED status remain available.
- WLED JSON is **private-LAN HTTP**, limited to manually entered RFC1918 IPv4 addresses. It saves master state plus segment 0 to a local recovery journal before each automatic scene. If restoration fails, no subsequent scene can fire while the journal is outstanding. The previous state may require you to reopen GlowDeck and manually restore.
- Android/Fire OS may kill a foreground service for memory pressure, battery policies, network changes, force-stop or sleep. **This cannot promise 100% continuous monitoring.** It is not a replacement for a continuously powered ESP32. Android 15+ `dataSync` restrictions can also affect future target-SDK upgrades; this test APK targets API 28 for Fire TV compatibility.
- Allow notifications if prompted. Some Fire TV variants do not visibly present ongoing notifications; the on-screen STOP control still works.
- On-demand WLED preview and NHL API tests are available even when no game is live, but live-goal monitoring requires a live matchup.
- **Hardware acceptance pending:** Build success is not proof it works on your Fire TV or WLED. Test device model/Fire OS, HOME switch, one full game, service/notification persistence, and restart/recovery. Avoid time-critical/safety-critical lighting. Disconnect power to WLED or use WLED's own web UI if recovery fails.

## Diagnostics

```powershell
adb shell dumpsys activity services com.gkowdeck.gameday.firetv
adb logcat -s GlowDeckMonitor GlowDeckGameDay
adb shell am force-stop com.gkowdeck.gameday.firetv
```

The final command **force-stops** the app and service and should be used only for intentionally testing interruption/recovery. Reopen GlowDeck after force-stop to retry a saved WLED restoration. On some devices, you can inspect service notification status in Fire TV's notification interface.

Source: `firetv/app/src/main/java/com/gkowdeck/gameday/firetv/GameDayService.java` and `MonitorRules.java`; the Activity is a UI controller, not the background network worker. Run `gradle --no-daemon :app:testDebugUnitTest :app:assembleDebug` in `firetv/`. Source remains under your `ggrant111/glowdeck-sports` repository.

## v0.4 built-in + custom audio

Choose **SOUND** in the Fire TV app. Audio starts OFF for safe coexistence with streaming apps. Enable sound if you want to hear original synthesized **Stadium Horn**, **Double Horn**, or **Siren**. Use NEXT SOUND per team to select a different source; **OFF** and **CUSTOM WAV** are also choices. Each team's profile is saved independently. Volume is adjustable in 10% steps and PREVIEW works even before arming a game.

To import a user-supplied clip, select **IMPORT WAV** for a team. On Fire TVs with a document picker, choose a WAV file; GlowDeck validates it and stores it in its app-owned folder as `UTA.wav`, `ANA.wav`, etc. Compatible audio: **PCM mono, 16000 Hz, 16-bit, max 10 seconds**, canonical 44-byte WAV header, with a maximum audio payload of 320 KB. Importing a WAV automatically selects the CUSTOM WAV profile for that team.

Some Fire TVs don't have a document picker. For ADB-assisted testing on compatible devices, use `adb push HORN1.wav /sdcard/Android/data/com.gkowdeck.gameday.firetv/files/gameday-audio/UTA.wav` after ensuring the app folder exists. On newer Fire OS builds, direct access to Android/data may be restricted; use a compatible document picker or app-local import workflow if the copy is refused.

For original test WAVs, run `python tools/make_test_wavs.py output` from the repository root. The provided Python tool produces three original, license-safe sample clips.

**Audio doesn't interrupt WLED restoration.** Goal audio starts on the same event after the selected TV delay as lights. If playback fails, WLED still restores after eight seconds. Custom missing WAV falls back to built-in Horn. In YouTube TV and other video apps, Fire OS may duck, mix or mute audio; foreground-service survival doesn't prove audio-focus compatibility. Test at low volume. No background sound is enabled automatically after install.

**v0.4 software evidence:** [Android `main` tests + debug APK build](https://github.com/ggrant111/glowdeck-sports/actions/runs/41383412450); Fire TV speaker, audio focus and custom import remain **hardware unverified**. Audio defaults OFF on a fresh app launch even if an earlier session enabled it.

## v0.4 celebration modes

- **AWAY:** celebrate only away goals in the away team's color.
- **HOME:** celebrate only home goals in the home team's color.
- **BOTH:** celebrate either team's newly detected goals using the scoring team's respective color.
- Mode is saved per NHL game ID, not changed while monitoring is active. Stop the monitor to switch mode.
- A 20-second cooldown, single pending scene, unique play event IDs, recovery journal, and eight-second restoration apply in all modes. Rapid goals during a busy scene or cooldown may be intentionally skipped. Always arm explicitly.

## v0.4.1 iPhone LAN Sound Manager

**[Full walkthrough and security notes](../docs/LAN_SOUND_MANAGER_V041.md).** Open the SOUND tab on Fire TV and keep it visible. It displays `http://FIRE_TV_IP:8788/` and a random eight-digit code. Open that URL in Safari on the same Wi-Fi, enter the code, then upload supported WAVs, assign sounds for all 32 teams, manage volume/mute and play previews through the television. Safari automatically converts decodable WAVs to mono 16 kHz, 16-bit PCM, up to 10 seconds. WAVs never leave the home network. Local server shuts down on leaving the SOUND screen; NHL monitoring continues separately.

**Build status:** New feature on `firetv-v0.4.1-lan-audio` branch; physical iPhone/Fire TV test still required. This replaces neither the APK's existing Fire TV file picker nor the ESP32 microSD workflow.
