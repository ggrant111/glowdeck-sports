# v0.5.2 — pregame arming, device paging, schedule and recovery

## Included

- ESP32 LIGHTS shows all eight supported WLED devices in three pages. Arrows
  select a page; selection is the underlying device index, not the visible row.
  Scan preserves existing order/selection. Additional discovered devices produce
  an explicit eight-device capacity message. Web setup can select/remove devices.
- ESP32 and Fire TV accept FUT/PRE and live games for arming; PPD remains waiting.
  While pregame, feeds establish/update a baseline without producing effects.
  An opening-period event after a fresh pregame baseline can produce its enabled
  effect. If first connected live, or recovering a stale feed, history is skipped.
- Today, Tomorrow and days +2 through +6 are selectable. ESP32 cycles dates by
  tapping the date button on SCORES; arrows on the right page through that day's
  games. Web setup and Fire TV expose seven date buttons. One day is fetched at
  a time to keep ESP32 memory bounded; this is not a combined week-long list.
- Scheduled times are shown locally. Fire TV uses its system timezone. ESP32
  defaults to UTC; its web setup offers UTC, Mountain, Pacific, Central or Eastern
  with daylight-saving rules. Network time must synchronize before future dates
  are available. Selected games are retained separately from the browsing list.
- The armed monitor polls the selected game ID directly across midnight and
  browsing dates. Unknown/missing feeds pause and retry, rather than erase it.
  CNCL stops; PPD waits; completion retains existing final-event/restore rules.
- Reboot resume is explicitly OFF by default. Enable in ESP32 SETUP/web or Fire
  TV GAME. Only a monitor that was armed is eligible. Saved game, team mode,
  target and delay are revalidated; old events are baselined, never replayed.
  Disarm/manual override clears the saved armed flag. Sound stays OFF after boot.
  Fire OS may block boot startup, force-stopped apps or background operation;
  the app records RESUME BLOCKED if starting the foreground service is rejected.
- The local browser mock supports upcoming selection/start, cancellation, eight
  devices, date cycling and simulated opt-in reboot resume. It does not access NHL
  or WLED and is not the separately hosted Lovable deployment.

The ILI9342 display rotation remains 2 and touch rotation remains 1. Other
controller profiles are retained. No idle full-screen repaint was reintroduced.

## Install

Download main or pull the repository. For Geoff's confirmed landscape profile,
close COM5 monitor and run from the project folder:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -e cyd_ili9342 -t upload --upload-port COM5
```

Fire TV APK is produced by the Fire TV test APK workflow and published as
`firetv-v0.5.2-test`. Debug signing behavior is unchanged; retain the prior APK
if an installation needs rollback. ESP32 artifacts distinguish all controllers.

## Hardware acceptance — record observed results

| Test | Expected result |
| --- | --- |
| Scan 4–8 WLED devices; go to last page | Every stored device accessible; selected device remains highlighted on return |
| Select device 8 and preview | Only that device changes; original scene restores |
| Arm FUT/PRE game | WAITING status; no light/audio changes; Away/Home/Both and TV delay retained |
| Leave armed until live | Transitions without another tap; first eligible new event triggers exactly once |
| Start monitoring an already-live game | Existing goals/period-start are not replayed |
| Disarm before start | No later automatic effect; no reboot resume |
| Browse another date while armed | Original game remains monitored by ID |
| Future-date selection/local start | Displayed time matches selected timezone and known schedule |
| Cross midnight | Original selection survives and monitoring continues |
| Lose Wi-Fi, including at game start | No stale celebration; restores when reachable; resumes from fresh baseline |
| Reboot with resume OFF | Disarmed; recovery journal restores any interrupted scene |
| Reboot with resume ON while armed | Exact saved target/game/mode resumes; no historical lights or sound |
| Cancelled/postponed/final fixtures | Cancelled stops, postponed waits, final stops after restoration/grace |
| Let screen idle | No periodic flash; state changes still update |

Live API access from the build environment returned HTTP 403. Parser fixtures,
state-machine tests and builds validate the implementation, not the live feed
or physical UI. NTP, timezone/DST display, device discovery, Fire OS boot handling,
and live opening-period timing require the hardware checks above.
