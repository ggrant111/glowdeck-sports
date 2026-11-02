# Test live NHL scores and a real WLED device before the ESP32 arrives

This is a **local, manual test bench** for GlowDeck GameDay. It runs with Python 3 (standard library only) on a computer connected to your home network. The **phone connects to that computer**; the computer contacts WLED. No cloud tunnel or account is needed. The ESP32 will later eliminate the need for the computer.

This separate test bench is deliberately **not** the published Lovable simulator. The Lovable demo at https://glowdeck-gameday-simulator.lovable.app now has a **Demo / Live NHL** data toggle, but its WLED is still mock-only. The local test bench includes **real NHL data plus manual, temporary WLED previews** on the same page. Automatic real-goal lighting remains disabled until it can be validated safely on hardware.

## First: check the WLED device

On a phone or computer connected to the same home Wi-Fi, try opening `http://192.168.1.71/` (the previously known address for the Fireplace WLED). If it no longer opens, get its **current IP** from the WLED app. Do not assume its IP never changes.

Test this on a spare strip or nonessential LED segment. Preview temporarily changes **segment 0**. No high-rate flashes or music effects are sent.

## Windows — terminal/PowerShell

1. Download or clone the repository at https://github.com/ggrant111/glowdeck-sports (Code → Download ZIP is fine). Extract it.
2. Install Python 3 if needed, then open PowerShell in the extracted repository folder.
3. Launch the service, replacing the WLED IP if it changed:

   ```powershell
   py -3 tools\gameday_bridge.py --wled-ip 192.168.1.71 --listen 0.0.0.0
   ```

   No `pip install` needed. The command prints a **random access code**. Keep PowerShell open while testing.
4. Run `ipconfig` and find the PC's Wi-Fi **IPv4 Address**, e.g. `192.168.1.50`.
5. On your iPhone, on the same Wi-Fi, open **Safari** and visit `http://192.168.1.50:8787/`, replacing the IP with your **computer's** address (not the WLED's). Enter the access code.
6. If Windows asks about network access, allow only **Private networks**. Do not forward port 8787 from your router.

## macOS/Linux

Run from the repository root:

```bash
python3 tools/gameday_bridge.py --wled-ip 192.168.1.71 --listen 0.0.0.0
```

Find the computer's LAN IP in Network/Wi-Fi settings (or `ipconfig getifaddr en0` on most Macs), then open `http://<computer-ip>:8787/` on your phone and enter the printed access code.

**On the same computer only:** omit `--listen 0.0.0.0` and open http://127.0.0.1:8787/. Default binding is loopback-only.

## Suggested testing sequence

| Test | Action | Expected result |
|---|---|---|
| NHL feed | Tap **Refresh NHL** | Today's real games; game-free day shows no games |
| Game selection | Tap a game in SCORES | GAME tab shows actual score and status |
| WLED read | Tap **Read WLED state** | Master power, brightness, segment-0 status (no changes) |
| Team preview | Choose a game, tap **Preview team color**, confirm | Real WLED segment 0 changes to selected team color for 5 seconds |
| State restoration | Wait 5 seconds | Original effect, brightness, colors and master on/off restored (where WLED supports state fields) |
| Restore manually | Tap **Restore now** during a preview | Previous WLED scene restored immediately |
| Recovery | End the process during preview, start again | Recovery journal is read and the old state restored, or retained for retry if WLED is offline |
| Safety | Score changes in the real NHL feed | **No automatic lighting commands** issued |

## Safety, permissions and limitations

- A 12+ character randomly generated access code is required on every API call, and cross-origin write requests are rejected. The test bridge has **no login/TLS**; run it only on trusted private Wi-Fi. Avoid guest/public Wi-Fi.
- The allowed WLED IP is configured explicitly at startup; the bridge does not provide an arbitrary URL fetch proxy or connect to other WLED IPs.
- Real NHL GET uses a fixed HTTPS endpoint, requests validated by the system's default TLS trust store, a 10-second timeout and a 25-second cache.
- A preview lasts **2–10 seconds**, sets brightness to **110/255**, uses solid team color and then restores a persisted state snapshot. If a restore fails, a recovery journal is retained at `~/.glowdeck-gameday/bridge-recovery.json` until a later retry.
- Segment 0 and master state are restored, but esoteric WLED usermods/presets/playlists and concurrent changes from other remotes may not be perfectly reversible. Do not test on essential or expensive lighting scenes.
- This tests the **NHL API → computer → LAN WLED** path, not the CYD screen, ESP32 Wi-Fi, or v0.2 firmware. It cannot establish that the firmware will work on the board.
- The standalone `tools/bridge_web/index.html` UI is rendered by the computer's built-in HTTP server; opening that file directly in iPhone's file viewer may again block scripting.
- Reconfigure the WLED IP before launching when DHCP changes it. If the iPhone cannot connect, first visit the URL on the computer itself, then check Windows firewall and same-LAN access.
- Stop the bridge with **Ctrl+C** when finished. The service attempts recovery on exit and on restart; if WLED was unavailable, restart later to retry.

## Automated offline tests

```bash
python3 -m unittest discover -s tools/tests -v
```

Tests cover NHL normalization, invalid scores and payloads, state snapshot/restore, segment safety, disallowed public IPs, invalid preview values, restart restoration, failed restoration preservation, and no automatic WLED action during score parsing.
