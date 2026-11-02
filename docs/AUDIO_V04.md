# GlowDeck GameDay v0.4 — Built-in and Custom Sounds

**Status: product/design specification ACCEPTED on 2026-11-01; not yet implemented or tested in either firmware or APK.** This is the cross-platform source of truth for GameDay audio behavior. See [Feature Parity](FEATURE_PARITY.md) and [Project Plan](../PROJECT_PLAN.md).

## Decision

Support **BOTH sound sources** rather than forcing the user to choose one global type:

1. **Built-in sound library**: short original synthesized stadium-style horn, beep, cheer-like sound effect and silent option. Must work without a microSD card or internet. "Onboard" means sounds generated/packaged on the device, **not** a guarantee of a built-in physical speaker.
2. **Custom sound library**: user-supplied, appropriately licensed WAV clips stored on a microSD card on the ESP32. Fire TV imports locally accessible user files; browser demo permits user-chosen local audio as a preview only.

The user can mix built-in and custom selections in one event profile: e.g., UTA goal = built-in horn; BOS goal = `/gameday/audio/BOS_GOAL.WAV`; victory = built-in cheer. An invalid/missing custom file must **fail safely to a chosen built-in fallback** (or silence if sound is disabled), while WLED lights still work.

## UI contract

- Shared **Sounds** screen/settings section: sound master on/off (default OFF for initial hardware bring-up), device volume 0–100% with capped output, Test speaker, Audio source [Built-in / Custom WAV / Off], source picker and **Preview Sound** action.
- Map each event and team to a separate sound: away goal, home goal, win, period start, intermission, power play. Only goal events enabled in v0.4; others can appear as disabled *coming soon* without taking misleading actions.
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

- [ ] Shared original procedural tone/horn preset with short, clearly non-copyrighted clips.
- [ ] Per-game Away/Home/Both goals map correct owner to correct sound asset.
- [ ] Built-in playback, custom file loading, mono WAV format validator and bounded task buffer.
- [ ] Independent mute and volume with persistence, mute on restart for initial tests.
- [ ] Missing/invalid WAV gracefully falls back while WLED continues unaffected.
- [ ] Cancellation and rapid goal cooldown do not leak audio playback or block 8-second WLED restoration.
- [ ] Simulator previews for built-in and custom audio; clearly label it as simulated.
- [ ] Automated tests on both Fire TV and ESP32 and cross-platform parity check extension.
- [ ] Physical CYD speaker/SPI/audio quality and volume tests after hardware arrives.
- [ ] Fire TV background audio focus and streaming-app coexistence verified separately.

Do not mark these as complete until code, CI and hardware evidence exist.
