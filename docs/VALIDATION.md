# Validation log — v0.1

## Verification results

- PASS: `scripts/check-native.sh` compiled the simulator with system g++ using `-Wall -Wextra -Werror` and executed it successfully.
- PASS IN GITHUB ACTIONS (2026-11-01): 7/7 PlatformIO native Unity tests, `pio run -e cyd` firmware compilation, and firmware artifact upload ([run 41364609955](https://github.com/ggrant111/glowdeck-sports/actions/runs/41364609955)). The current local sandbox still does not have PlatformIO.
- NOT RUN: actual CYD and WLED hardware testing.

## Automated checks available

- Portable core: 7 Unity tests under PlatformIO `env:native`.
- Native C++ deterministic simulator: 5 game frames, exactly 2 display alerts, 0 automatic WLED commands.
- GitHub Actions workflow: intended to compile/test all commits and upload compiled firmware as an artifact.

## Not verified in this environment

- **Physical flash / boot** (firmware has compiled in GitHub Actions, but has not been uploaded to a real CYD).
- Physical ILI9341 TFT and XPT2046 touch mapping/calibration for a specific board revision.
- First-boot captive portal and reliable 2.4 GHz Wi-Fi connectivity.
- Live internet NHL HTTPS redirect/JSON payload response at runtime.
- WLED mDNS discovery and actual LED state change in a user's LAN.

## Known limitations

- Scoreboard only (not goal scorer, power plays or major plays). NHL updates are polling-based, not realtime push.
- Up to 16 NHL games/day displayed (4 per screen). Up to 8 WLED devices saved; first 3 shown on touchscreen; remaining devices can be managed on web UI but not selected from touchscreen in v0.1.
- TLS certificate validation is disabled for NHL proof-of-concept requests.
- WLED API controls segment 0 only, no segment backup/restore and no automatic score effects.
- The default touch transform is approximate. Device-specific calibration is required.
- Wi-FiManager setup blocks normal input while portal is active; after timeout the UI recovers and Wi-Fi can reconnect.
- Local web setup lacks authentication/HTTPS; do not port-forward it.

## v0.2 milestone (2026-11-01)

- PASS: [CI run 41368433335](https://github.com/ggrant111/glowdeck-sports/actions/runs/41368433335) — 17/17 native Unity cases, ESP32 PlatformIO compile and firmware artifact upload.
- PASS: desktop v0.2 mock goal cycle, including restore.
- NOT RUN: CYD display, WLAN, actual NHL response and real WLED state capture/restore. See [v0.2 validation](VALIDATION_V02.md).

## Fire TV test APK v0.1 — separate Android application (2026-11-01)

- PASS: Java-only smoke checks for GoalTracker initial baseline, deduplication, one new goal and RFC1918 IP restrictions.
- PASS: [GitHub Actions run 41372267341](https://github.com/ggrant111/glowdeck-sports/actions/runs/41372267341) — Android SDK, native JVM unit tests, debug APK compile, artifact + release steps succeeded.
- NOT RUN: Fire TV launcher/D-pad test, actual NHL HTTPS requests from Fire OS, actual WLED LAN communications, effect restoration while interrupted, real-live-game reactions.
- The APK is a temporary debug-signed prerelease. Full hardware acceptance required before making claims about live event or WLED reliability.

- PASS (final Fire TV v0.1 build): [CI run 41372660218](https://github.com/ggrant111/glowdeck-sports/actions/runs/41372660218) completed native JVM tests, Android APK compile, checksum and test-release publishing. [Download APK](https://github.com/ggrant111/glowdeck-sports/releases/download/firetv-v0.1.0-test/GlowDeck-GameDay-FireTV-v0.1.apk). WLED preview can now use UTA blue/BOS gold without a selected game. Fire TV/WLED hardware acceptance still NOT RUN.

## Fire TV background v0.2 (2026-11-01)

- PASS: Pure Java safety-rule smoke checks (7 assertions) for disarmed conditions, favorite ownership, restore lock, cooldown, stale feed and bounded delay.
- PASS: [GitHub Actions build 41373942957](https://github.com/ggrant111/glowdeck-sports/actions/runs/41373942957) — Android JVM unit tests, Fire TV debug APK assembly, SHA256 file creation and release publishing.
- [Download v0.2 APK](https://github.com/ggrant111/glowdeck-sports/releases/download/firetv-v0.2.0-test/GlowDeck-GameDay-FireTV-v0.2.apk); source: `firetv/app/src/main/java/com/gkowdeck/gameday/firetv/GameDayService.java`.
- NOT RUN: Real Fire TV background survival with YouTube TV/Prime/ESPN; foreground notification lifecycle and STOP action; real NHL goal and WLED state restoration; OS kill/network-loss tests. This feature is not hardware verified.

- USER HARDWARE CHECK (2026-11-01): Fire TV displayed ARMED after switching to YouTube TV. AirAPK `dumpsys activity services` showed `GameDayService`, `isForeground=true`, and `foregroundId=1227`, roughly 7m41s elapsed. The real WLED eight-second manual test worked. The only current NHL game was in intermission. NHL polling continuity, automatic goal reaction and exact scene restoration still require separate observation.

- PASS, log evidence, 2026-11-01: OTT vs ANA, favorite ANA, 0s delay. ARMED 19:06:28; GOAL WAIT 19:06:53.210; GOAL LIGHTS 19:06:53.897; restored and ARMED 19:07:02.100 (about 8.2s). Physical appearance and exact scene restoration still require visual confirmation.

- PASS, user-observed physical lighting (2026-11-01): During OTT @ ANA, the WLED controller changed to Anaheim orange for a real goal and then visibly returned to the prior appearance. User also observed another live Anaheim goal trigger WLED while YouTube TV was playing on the Fire TV. Two real goal-triggered reactions reported; complete background path validated for this device. Still pending: exact WLED JSON state comparison, interruption/recovery tests and extended multi-game reliability.

## Fire TV v0.3 — celebration modes (2026-11-01)

- PASS: [GitHub Actions run 41377858153](https://github.com/ggrant111/glowdeck-sports/actions/runs/41377858153) completed Android JVM tests, APK compile, checksum, upload and prerelease publishing.
- PASS (code/test): mode logic validates AWAY-only, HOME-only, BOTH, unknown owner, invalid stored mode, duplicate suppression, cooldown/busy policy, and ambiguous multi-goal suppression.
- Feature unverified on the user's physical Fire TV until sideloading the v0.3 APK and testing both-team goals.
- APK: [GlowDeck-GameDay-FireTV-v0.3.apk](https://github.com/ggrant111/glowdeck-sports/releases/download/firetv-v0.3.0-test/GlowDeck-GameDay-FireTV-v0.3.apk).

## ESP32 v0.3 PBP parity — software acceptance (2026-11-01)

- New native Unity tests: away/home/both, scoring-side color, duplicate/ambiguous new IDs, incorrect team IDs, post-outage baselining, TV delay and eight-second WLED restoration.
- Cross-platform Python parity check compares 32 team-color RGB codes, UI modes, NHL goal IDs, polling intervals, cooldown and restore conventions against the working Fire TV implementation.
- GitHub Actions firmware build and native tests **PASSED on development branch** (initial run 41379355680). Final post-documentation commit and main-branch build separately tracked.
- NOT RUN ON PHYSICAL CYD: touch calibration, PBP filter heap/resource usage, Wi-Fi, TLS correctness, WLED discovery, complete eight-second scene restoration, reboot recovery, and multi-hour soak. Never claim this is hardware-verified.

- PASS (ESP32 v0.3 `main` firmware): [Actions run 41380024037](https://github.com/ggrant111/glowdeck-sports/actions/runs/41380024037) passed **23/23** native tests, compiled ESP32-2432S028R firmware and uploaded `GlowDeck-GameDay-v0.3-firmware` artifact. This is not physical hardware validation.
- PASS (cross-platform parity): [Actions run 41380024002](https://github.com/ggrant111/glowdeck-sports/actions/runs/41380024002) verified matching 32 NHL RGB values, Away/Home/Both modes, NHL PBP goal IDs, eight-second effects, 20-second cooldown and restore hooks.
- Hosted simulator https://glowdeck-gameday-simulator.lovable.app published with v0.3 Away/Home/Both behavior. Lovable reported 21 tests and emulated 390×844 mobile checks passing. Actual Safari and ESP32 hardware tests pending.

## v0.4 original + custom audio (2026-11-01)

- PASS on feature branch: [ESP32 CI run 41382843599](https://github.com/ggrant111/glowdeck-sports/actions/runs/41382843599) — **28/28 native tests passed** (including WAV header/name/size checks), CYD firmware compiled and artifact uploaded. GPIO26 speaker and microSD streaming have **not** been tested physically.
- PASS on feature branch: [Fire TV CI run 41382441636](https://github.com/ggrant111/glowdeck-sports/actions/runs/41382441636) — Android audio-model JVM tests and native APK compile passed. Background playback/audio focus and custom-file import **not yet tested on physical Fire TV**.
- A v0.4 cross-platform parity workflow now checks three built-in horns, custom WAV validator, team-specific goal-triggered sounds, safe default audio OFF, touchscreen/TV UI and browser mock.
- Physical hardware acceptance OPEN: confirm CYD amplifier GPIO26, speaker polarity on amplifier outputs, full WAV playback, SD/touch bus restoration, OS-level audio focus in YouTube TV, and continued WLED restoration after a goal.

## v0.4 final `main` release results

- PASS: [ESP32 CI run 41383412590](https://github.com/ggrant111/glowdeck-sports/actions/runs/41383412590) — 28/28 native cases, CYD firmware compiled and `GlowDeck-GameDay-v0.4-firmware` artifact uploaded. Flash 1,203,973 / 1,310,720 bytes (91.9%), RAM static 51,196 / 327,680 bytes (15.6%). Firmware compilation is not proof of physical speaker playback or safe touch/SD concurrency.
- PASS: [Fire TV CI run 41383412450](https://github.com/ggrant111/glowdeck-sports/actions/runs/41383412450) — Android JVM tests, APK compilation, checksum and GitHub test prerelease. APK: [GlowDeck-GameDay-FireTV-v0.4.apk](https://github.com/ggrant111/glowdeck-sports/releases/download/firetv-v0.4.0-test/GlowDeck-GameDay-FireTV-v0.4.apk). Sound master safely resets OFF on fresh Activity creation.
- PASS: [Cross-platform audio parity run 41383412448](https://github.com/ggrant111/glowdeck-sports/actions/runs/41383412448) — original horns, custom WAV validation, team-specific audio selection, safe mute and demo checks.
- Browser: hosted [GlowDeck v0.4 simulator](https://glowdeck-gameday-simulator.lovable.app) reports 43 builder tests and Chromium mobile layout checks. **Not personally tested on iOS Safari**.
- NOT RUN: actual speaker on CYD, custom WAV microSD transfer/read, hardware audio fidelity/volume, touch response while SD playback active, Fire TV native audio focus/mixing while YouTube TV is streaming, real combined audio+WLED live goal.

## 2026-11-01 — Live Fire TV v0.4 goal audio + WLED field test (user reported)

- **PASS — firsthand user report:** An Anaheim Ducks goal during an actual NHL game triggered **both audible goal sound and WLED goal lighting** under GameDay v0.4. This is the first user-confirmed combined live goal/audio/light result for the Fire TV build.
- **Evidence level:** User-observed result, not an independently captured video, Android logcat, or timestamped NHL event trace. Do not infer exact detection delay or team preset, or claim that both teams/custom WAV were tested.
- **USER FIELD PASS (follow-up):** The goal sound played while the user was watching **YouTube TV**, and WLED visibly returned to its previous scene afterward. This confirms real background audio playback alongside streaming video and visible WLED restoration.
- **Still to verify:** Exact elapsed eight-second restoration and WLED JSON equivalence, video/audio ducking or playback interruptions, custom WAV import, multiple-game endurance, and network recovery.
- **ESP32 remains untested on physical hardware**: this result validates the Fire TV path, not the CYD speaker or microSD/audio pipeline.

## Fire TV v0.4.1 LAN sound manager — initial software checks (2026-11-01)

- SOURCE IMPLEMENTED: mobile-first LAN page served by Fire TV over `http://FIRE_TV_PRIVATE_IP:8788/`, eight-digit pairing code displayed on SOUND tab; uploads are WAV-only, max 320044 bytes, validated to canonical PCM mono 16000Hz/16-bit.
- JVM tests added: 32-team whitelist, invalid path/name rejection, pairing code matching, private-LAN Host checks, Content-Length limits and canonical WAV header acceptance.
- v0.4.1 CI check and real Fire TV/iPhone validation remain open; no claim that an actual iPhone connected or uploaded a WAV.
- Audio and NHL/WLED events remain separate. Browser playback test never sends a lighting command. Untrusted/public Wi-Fi and port-forwarding are explicitly unsupported.

## Fire TV v0.4.1 official `main` software verification (2026-11-01)

- PASS: [Fire TV Android JVM tests, APK compile/sign/asset release 41386294301](https://github.com/ggrant111/glowdeck-sports/actions/runs/41386294301). [Download APK](https://github.com/ggrant111/glowdeck-sports/releases/download/firetv-v0.4.1-test/GlowDeck-GameDay-FireTV-v0.4.1.apk). Unit tests cover team whitelist, PIN validation, private Host checks, bounded Content-Length and canonical WAV files.
- PASS: [Cross-platform parity 41386294313](https://github.com/ggrant111/glowdeck-sports/actions/runs/41386294313) checks Fire TV LAN pairing/size/preview and existing ESP32/browser sound paths.
- PASS: [ESP32 firmware regression 41386294269](https://github.com/ggrant111/glowdeck-sports/actions/runs/41386294269) — native tests and CYD compile, with no firmware behavior change.
- PASS: Mobile web page JavaScript syntax checked, controls present. Not a browser-on-iPhone test.
- NOT RUN: actual iPhone-to-Fire TV TCP/HTTP connection, PIN rejection/success, WAV upload/preview, physical audio and game-active rejection, long-term availability. These remain open in [LAN_SOUND_MANAGER_V041.md](LAN_SOUND_MANAGER_V041.md).
