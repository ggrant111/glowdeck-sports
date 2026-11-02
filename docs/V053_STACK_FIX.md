# ESP32 v0.5.3 — loopTask stack overflow fix

The supplied log repeatedly reports `Stack canary watchpoint triggered (loopTask)`
after Wi-Fi connects, before the first scoreboard result. It also reports
`Display rotation=2 size=320x240`: the intended profile is installed.

The v0.5.2 NHL request chain held caller and callee score/feed buffers plus
1.5–2 KiB JSON filters on the Arduino task stack, alongside HTTPS/TLS calls.
The pinned Arduino ESP32 2.0.11 core defaults this stack to 8192 bytes.
The exact crashing address was not symbolicated against the user's matching ELF;
the log establishes stack exhaustion, while the request path is inferred from
the timing and inspected stack allocations.

Changes:
- Use the pinned core's `SET_LOOP_TASK_STACK_SIZE` override for 16384 bytes.
- Move score/feed scratch buffers to static storage. NHL operations are serialized
  on loopTask; these scratch buffers must not be called concurrently.
- Allocate JSON filter storage on the heap with explicit allocation checks.
- Print firmware version/stack budget at boot and stack high-water/free-heap
  diagnostics after each scoreboard request.
- Preserve rotation 2, touch mapping, NVS/Wi-Fi settings and all v0.5.2 features.

All controller profiles receive the fix. The Fire TV APK remains v0.5.2 because
this change is specific to ESP32 memory allocation. Build success alone cannot
prove hardware stack margin; verify on the board.

## Acceptance

Upload `cyd_ili9342` without erasing flash. Look for:
`GlowDeck ESP32 v0.5.3 - loop stack 16384 bytes`, then Wi-Fi connected,
`NHL memory: stack minimum free=...`, and an NHL games result without rebooting.
Leave running for at least ten scoreboard polls, browse future dates, scan WLED,
arm a game, and test reboot resume if enabled. Report the minimum stack-free and
heap-free values plus any panic. A failure response should pause/retry normally,
not reboot. The missing Preferences keys in the earlier log are separate and
are not evidence of corrupt Wi-Fi settings; do not erase NVS to address this crash.
