# v0.5.4 — time display preference

ESP32 web setup: under Schedule, choose **12-hour (7:00 PM)** or
**24-hour (19:00)**, then **Save time format**. Selection persists in NVS,
reflects the saved choice when the web page reloads, and redraws upcoming game
times immediately. The timezone and game/event timing are unchanged. Existing
ESP32 devices default to their previous 24-hour display until changed.

Fire TV: the SCORES/SCHEDULE page has a **TIME FORMAT** toggle. It persists
independently on the Fire TV; the default is 12-hour. The browser mock exposes
the same display choice in SETUP. These are per-device preferences.

Validation: clock checks cover midnight, noon, leading minutes, AM/PM and
23:59. CI builds all three ESP32 profiles and the Fire TV APK. Device acceptance:
select 12-hour in web setup, confirm AM/PM on Scores and Game, reboot and verify
the choice persists, then switch back to 24-hour. Verify on-screen labels fit.
Rotation 2 and the v0.5.3 stack fix are retained for the ILI9342 profile.
