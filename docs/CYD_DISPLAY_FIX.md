# CYD display and NHL streaming fix

The reported board is marked ESP32-2432S028 and has micro-USB plus USB-C.
The front photo shows clipped width and an unpainted/garbled strip, consistent
with a display address-space mismatch. Connector type alone cannot identify
the controller. `cyd_ili9342` is a **hardware test profile**, not a confirmed
identification of this board.

- `cyd`: existing portrait-native ILI9341, rotation 1, logical 320x240.
- `cyd_ili9342`: landscape-native ILI9342, rotation 2, logical 320x240, RGB order.
  This changes the controller address mapping by 90 degrees while retaining
  the application's landscape layout. DDP, audio and sports features are identical.

## Install the test profile on COM5

Close the serial monitor first. From the repository on `feature/ddp-event-effects`:

```sh
git pull --ff-only
pio run -e cyd_ili9342 -t upload --upload-port COM5
pio device monitor --port COM5 --baud 115200
```

The user confirmed that touch already targets the correct landscape positions.
Both profiles therefore retain the original touch rotation and default mapping;
there is no automatic calibration. Only the graphics controller mapping changes.
Optional calibration is available if a different panel needs it: open the serial
monitor, press reset, and send `C` when the boot message requests it, then tap
and release the three crosshairs. The mapping handles swapped or reversed axes
and is saved separately for each display profile. Each target times out after
20 seconds; timeout retains the previous/default mapping. No flash erase is needed; Wi-Fi settings are retained.
To return to the old display profile, upload with `-e cyd` instead.

The firmware prints its rotation and logical dimensions at boot. Confirm the
entire 320x240 layout is visible, the garbled strip is gone, colors look correct,
and all touch tabs work. A successful compile does not establish panel compatibility.
If the test profile still renders incorrectly, inspect the panel/controller
identification rather than assuming every dual-USB board uses ILI9342.

## NHL transport

Both score and play-by-play requests now use HTTP/1.0 before GET. Parsing
`HTTPClient::getStream()` bypasses HTTPClient's chunk decoding. Chunk framing
can therefore be parsed as a scalar instead of the JSON object, producing
`NHL response missing games` despite HTTP 200. This is a plausible cause of
the reported log; the actual response from the board was not captured.
ArduinoJson documents this mitigation:
https://arduinojson.org/v6/how-to/use-arduinojson-with-httpclient/

Filter allocation failures now produce explicit errors. An empty games array
continues to be valid; missing games is not silently treated as an empty slate.
The first-boot Preferences NOT_FOUND messages are separate from this failure.

Validation: portable simulations, touch transform checks (swapped/reversed axes,
center point, degenerate calibration and bounds) and parity checks passed locally.
CI compiles both display profiles. Real board orientation, touch and NHL response
validation are still required. The existing Fire TV v0.5.0 APK is unaffected.

## Field correction: upside-down graphics and idle redraw

The user confirmed that the landscape-native profile displays the full UI but
its graphics are 180 degrees opposite the already-correct touch targets.
`cyd_ili9342` now uses display rotation 2 instead of 0. The touch controller
remains at rotation 1; its calibration namespace stays `landscape` for either
even display rotation, preserving any saved map. Boot should report
`Display rotation=2 size=320x240`.

The three-second UI poll used to clear and repaint the screen unconditionally.
Rendering now compares a value snapshot (including page, scores, device names,
banners and live status) and skips identical frames. Changed data, page switches,
audio completion and expired banners still update on the existing schedule.
Calibration invalidates the snapshot so returning to the UI repaints it.
Actual changes still use the existing full-frame drawing path; this removes
idle periodic flashes, not every possible flash during a real screen change.
