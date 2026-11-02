# GlowDeck GameDay Fire TV v0.4.1 — LAN Sound Manager

**Status:** Source implemented, CI verification pending; **not physically tested on a Fire TV or iPhone yet**. This is an on-device web server, **not** a public/cloud website. Fire TV remains the device that stores WAVs and plays horns.

## What it does

- Open a mobile-friendly sound management page on iPhone, Android or PC at `http://FIRE_TV_IP:8788/`.
- Change the original **Off / Stadium Horn / Double Horn / Siren / Custom WAV** source for any of 32 NHL teams.
- Upload a WAV file from your phone directly to Fire TV. The phone browser converts supported WAV formats to **PCM 16 kHz, 16-bit, mono** locally, truncating clips over 10 seconds. The server validates the exact 44-byte canonical header and 320 KB maximum audio payload before saving.
- Save a team-specific file in the Fire TV app-owned directory as `ANA.wav`, `UTA.wav`, etc. The upload automatically selects Custom WAV for that team.
- Adjust master sound on/off and 0–100% volume. **Test on Fire TV** plays the selected sound from the actual TV speaker without needing a game to be live and without commanding WLED.
- Preferences and imported WAVs persist. **The background-sound master resets OFF on a fresh app launch** for safety.

## Setup on Fire TV and iPhone

1. Install [v0.4.1 Fire TV test APK](https://github.com/ggrant111/glowdeck-sports/releases/tag/firetv-v0.4.1-test) (download the `.apk` asset, not source). This uses the unchanged `com.gkowdeck.gameday.firetv` package.
2. Connect iPhone and Fire TV to the **same home Wi-Fi/LAN**. Guest Wi-Fi or access-point isolation can prevent local connections.
3. Open GlowDeck on Fire TV, choose the **SOUND** tab, and leave it on screen. It shows the local Safari URL, for example `http://192.168.1.24:8788/`, and an **eight-digit pairing code**. Use the actual URL on your TV; IPs can change.
4. On iPhone, open the URL in **Safari** (plain HTTP, not HTTPS). Enter the eight-digit code printed on the Fire TV screen and select **Connect**.
5. Choose a team, set a built-in preset and press **Save team sound**; use **Play on Fire TV** to hear the result. For a custom horn, choose a local WAV and select **Upload to Fire TV**; the file is converted by Safari if needed and assigned to that team. Then select **Play on Fire TV**.
6. Enable **Sound ON** on the manager or TV when you want goals to make sound, select Away/Home/Both on the TV, and arm NHL monitoring. **Leaving the SOUND screen stops the upload server, not the already-running NHL goal service.** Your imported file remains available when you return to YouTube TV.

If the normal Fire TV file picker does not work, this local page replaces the need for ADB file transfer.

## Constraints & security

- LAN web server runs only while the Fire TV **SOUND tab is visible**. It starts on TCP **8788** and closes when you switch tabs, press Home or open another Fire TV app. It never listens continuously in the background.
- It accepts requests only from a private IPv4 LAN address and requires a fresh random eight-digit pairing code displayed on the TV. All reads of sound preferences and all writes/previews require that code; mutating calls additionally use a custom request header with same-origin browser handling.
- The LAN connection is **unencrypted HTTP**; pairing codes and files are visible to someone capable of sniffing that home network. **Do not expose/port-forward port 8788 or use untrusted Wi-Fi**. For greater security on untrusted networks, use no sound uploads until an encrypted transport is available.
- A valid team must match the 32 NHL abbreviations, not a user-supplied path. File upload uses a fixed `TEAM.wav` destination, bounded streaming, strict RIFF/PCM header validation, size limit and a temporary file before installation.
- Sound settings/import and remote preview are blocked while the NHL foreground monitor is **running or stopping**, so changing a live goal's sound cannot interfere with scene restoration.
- No actual NHL data, WLED changes, or cloud upload is sent by the sound-management page. The **existing** NHL monitoring and goal-to-WLED flow stays unchanged.
- iPhone file decoding/conversion supports WAV encodings the browser itself can decode. An unsupported WAV requires conversion on a computer. Converting is done entirely in the phone browser.
- You must explicitly enable audio again after a fresh Fire TV Activity launch. PREVIEW is permitted with master sound OFF; actual background goal audio is not.

## Testing checkpoints

- [x] Java source and phone UI authored: on-device web service, pairing, per-team status/assignment, bounded upload, preview, mute and volume.
- [x] JVM tests cover team whitelist, pairing-code checks, request length, RFC1918 host checking and canonical WAV acceptance.
- [ ] v0.4.1 GitHub Actions test and APK build pass on `main`.
- [ ] Physical Fire TV/iPhone: Safari opens the LAN URL on home Wi-Fi, pairing works and incorrect code is rejected.
- [ ] Upload a custom Anaheim WAV, preview on the TV and confirm it is still selected after reopening GlowDeck.
- [ ] Stop the manager by leaving SOUND; verify `http://FIRE_TV_IP:8788/` no longer responds.
- [ ] Monitor a live game in YouTube TV with a custom goal horn, confirm no video disruption and WLED restores exactly as before.
- [ ] Test malformed/WAV-too-large uploads, file overwriting and Wi-Fi interruption recovery.

## Cross-platform parity

This release adds the same **sound library and team assignments** to the Fire TV's LAN manager interface; browser simulator already supports local-file audio preview and team choices. The ESP32 currently exposes SOUND settings on its touchscreen/local webpage and plays WAVs copied onto microSD, but **does not yet offer authenticated browser-to-SD WAV uploads**. Adding that requires a secure PIN display, safe SD/touch SPI arbitration and on-device hardware testing. It is an explicit pending parity item, not a hidden claim of equal features.

See [FEATURE_PARITY.md](FEATURE_PARITY.md) and [AUDIO_V04.md](AUDIO_V04.md).
