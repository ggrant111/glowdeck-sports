# GlowDeck GameDay v0.4 — Built-in and Custom Sounds

**Status: v0.4 source IMPLEMENTED and CI-VERIFIED on `main` for ESP32, Fire TV and parity. Physical audio output and Fire TV streaming-app audio focus NOT TESTED.** This is the cross-platform source of truth for GameDay audio behavior. See [Feature Parity](FEATURE_PARITY.md) and [Project Plan](../PROJECT_PLAN.md).

## Decision

Support **BOTH sound sources** rather than forcing the user to choose one global type:

1. **Built-in sound library**: short original synthesized stadium-style horn, beep, cheer-like sound effect and silent option. Must work without a microSD card or internet. "Onboard" means sounds generated/packaged on the device, **not** a guarantee of a built-in physical speaker.
2. **Custom sound library**: user-supplied, appropriately licensed WAV clips stored on a microSD card on the ESP32. Fire TV imports locally accessible user files; browser demo permits user-chosen local audio as a preview only.

The user can mix built-in and custom selections in one event profile: e.g., UTA goal = built-in horn; BOS goal = `/audio/BOS_GOAL.wav`; victory = built-in cheer. An invalid/missing custom file must **fail safely to a chosen built-in fallback** (or silence if sound is disabled), while WLED lights still work.

## UI contract

- Shared **Sounds** screen/settings section: sound master on/off (default OFF for initial hardware bring-up), device volume 0–100% with capped output, Test speaker, Audio source [Built-in / Custom WAV / Off], source picker and **Preview Sound** action.
- Map each event and team to a separate sound: away goal, home goal, win, period start, intermission, power play. Only **goal events** are implemented in v0.4; the other event types remain future milestones.
- The **AWAY / HOME / BOTH** NHL goal selector continues to determine which scoring events qualify. Sound source is a distinct choice, and each qualified scoring team uses its own assigned sound.
- Store a selected sound per team/event (not only a single global selection); default built-in stadium horn for goals; inherit a global default if no team override exists.
- **Try sound** must work while GameDay is disarmed and must not send WLED events or alter NHL monitoring. Volume slider displays and uses the same value as the audio output engine.
- Surface errors on screen: SD missing, invalid WAV, clip too large, output unavailable, or playing interrupted. Provide a clearly labeled audio-output test independent of the network.

## ESP32 hardware & formats

- Target ESP32-2432S028R CYD common variant: **GPIO26 DAC2** routes through on-board speaker amplifier to **P4/SPEAK 2-pin JST (1.25 mm)** on compatible board revisions. Not all boards include the same speaker circuit. Confirm pinout and amplifier on physical board before enabling audio. Start with compatible passive **8-ohm speaker** on speaker connector; never assume either amplifier speaker output is ground, and do not connect a speaker directly to GPIO26.
- Use ESP32's **I2S DAC left channel / GPIO26** for sample playback where supported. No I2S channel conflict with touch pin GPIO25. Built-in tones can be synthesized in software and sent through same DAC path; do **not** operate independent PWM and DAC drivers simultaneously.
- microSD (separate SPI bus, **CS GPIO5, SCK18, MISO19, MOSI23**) stores small custom clips. Initial v0.4 support: uncompressed PCM **mono 16-kHz, 16-bit WAV**, short clips (suggest 1–10 seconds). Provide explicit format-validation and size limit; no arbitrary MP3 decoder or network audio required.
- Use a small bounded buffer and asynchronous/nonblocking playback scheduler (task/DMA where feasible), not WAV entire-file loading. Keep NHL polling, WLED scene timers/restore, touchscreen and Wi-Fi responsive; avoid unbounded allocation.
- Handle card removal, reboot, slow card, and repeated goals without blocking or losing lighting restoration; start muted by default for safety during hardware bring-up.

## Cross-platform behavior

| Feature | ESP32 | Fire TV | Browser demo |
|---|---|---|---|
| Built-in original horn | ESP32 DAC/audio task | Android sound asset/synthesis | Original synthesized preview |
| Custom sound | WAV from microSD | Local imported WAV (subject to platform permissions) | User-selected local WAV preview, never uploaded without consent |
| Team/event mapping | NVS config + SD path | Android preferences + app-accessible file | Local browser state, transient chosen file |
| Volume/mute | Independent from WLED | Independent from WLED; obey Android audio focus | Browser local volume slider |
| Goal-triggered playback | Same validated NHL goal event and TV delay as WLED | Same scoring-side event; background playback opt-in, respect streaming-app audio focus | Demo-only goals, no real WLED |
| Safe failure | WLED continues if audio fails | WLED continues if audio fails | Mock continues if sound unavailable |

**Important Fire TV constraint:** Background audio while watching YouTube TV or other streaming apps can interrupt, duck or compete with the streaming app. Default background sound **OFF**; require user opt-in and test Android audio focus behavior. Do not claim guaranteed mixing or playback priority.

## Initial development / test gates

- [x] Three original procedural horn presets on both platforms, plus Python WAV generator.
- [x] Goal owner selects Away/Home profile, consistent with WLED eligibility and TV delay. Profiles currently persist per **team abbreviation** rather than per-game (intentional for reuse).
- [x] Source implemented; mono PCM WAV validation and bounded stream buffers compile and pass tests.
- [x] Master background audio is OFF after restart on both platforms; volume and per-team kinds persist.
- [x] Fallback code paths implemented on both platforms. Physical playback behavior unverified.
- [x] Audio uses an independent worker with stop-on-restore hooks. Stress/physical concurrency testing still needed.
- [x] Local browser audio mock and hosted simulator update authored. Actual iOS Safari audio output unverified.
- [x] ESP32 WAV validation, Fire TV WAV validation, and static parity checks pass in CI on feature branch.
- [ ] Physical CYD speaker/SPI/audio quality and volume tests after hardware arrives.
- [ ] Fire TV background audio focus and streaming-app coexistence verified separately.

Do not mark these as complete until code, CI and hardware evidence exist.

## v0.4 actual implementation and how to test

**ESP32:** `AudioPlayer.cpp` uses I2S/DAC2 GPIO26 output and a dedicated FreeRTOS audio task. Custom audio is read in small chunks from microSD `/audio/NAME.wav`. The SOUND page supports selecting 3 built-in synthesized horns, CUSTOM WAV or OFF independently for Away/Home. Web setup adds a custom file-name form. Audio master is OFF after every boot; volume persists. Missing/invalid file falls back to built-in Horn. Any goal-generated audio stops when WLED restoration finishes.

**Fire TV:** `GameAudio.java` uses Android AudioTrack and an isolated thread; `AudioProfile.java` validates sound sources and canonical WAV. The new SOUND tab can enable background sounds, adjust volume, preview either team's sound, and import an audio file into app storage. Team settings persist per three-letter abbreviation. Optional audio runs independently of GameDayService's WLED worker, and is stopped during scene restoration or disarming.

**Browser mock:** `simulator/web/index.html` contains standalone Web Audio original horns, volume, mute, user-selected local WAV and both-team demo goals. This is not live NHL-to-real-WLED control.

**Test file generator:** Run `python tools/make_test_wavs.py output`. On ESP32, copy the WAVs into the microSD `/audio/` directory and enter their basename (`HORN1`, etc.) in the local device web interface. On Fire TV, import via the SOUND tab (device document picker required). All audio is original and generated from mathematical waveforms.

**Limitations:** ESP32 touch temporarily stops polling while custom microSD audio streams over VSPI, and restarts after completion; the display remains visible, WLED and NHL are intended to remain active. GPIO26 amplified speaker output must be physically confirmed for a specific CYD revision. Fire TV audio mixing/focus is OS-dependent and hardware-unverified. Uploaded audio is not hosted on a server. Never assume flash/build success proves an audible speaker output.

**Software verification:** [ESP32 firmware and 28 native tests](https://github.com/ggrant111/glowdeck-sports/actions/runs/41382843599); [Fire TV Android unit tests and APK build](https://github.com/ggrant111/glowdeck-sports/actions/runs/41382441636). The physical acceptance checklist remains open.

## Official v0.4 software release evidence

- [28/28 ESP32 tests and firmware](https://github.com/ggrant111/glowdeck-sports/actions/runs/41383412590); compiled binary is downloadable via the GitHub Actions firmware artifact. Upload with PlatformIO, not firmware.bin at flash address 0.
- [Fire TV audio JVM tests and v0.4 APK publishing](https://github.com/ggrant111/glowdeck-sports/actions/runs/41383412450); [download the Fire TV v0.4 APK](https://github.com/ggrant111/glowdeck-sports/releases/download/firetv-v0.4.0-test/GlowDeck-GameDay-FireTV-v0.4.apk).
- [Shared v0.4 audio parity CI](https://github.com/ggrant111/glowdeck-sports/actions/runs/41383412448) passed, including team choice, built-in/custom source and mute defaults.
- [Hosted sound simulator](https://glowdeck-gameday-simulator.lovable.app) published. Build provider reported 43 automated tests, but real Safari audio and physical lights remain unverified.
- v0.4 ESP32 flash usage 91.9% of partition, so budget future additions carefully. Speaker circuit and shared VSPI microSD/touch bus require physical validation.
