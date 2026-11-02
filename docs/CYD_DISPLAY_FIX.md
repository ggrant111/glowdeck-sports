# CYD display and NHL streaming fix

The reported board is marked ESP32-2432S028 and has micro-USB plus USB-C.
The front photo shows clipped width and an unpainted/garbled strip, consistent
with a display address-space mismatch. Connector type alone cannot identify
the controller. `cyd_ili9342` is a **hardware test profile**, not a confirmed
identification of this board.

- `cyd`: existing portrait-native ILI9341, rotation 1, logical 320x240.
- `cyd_ili9342`: landscape-native ILI9342, rotation 0, logical 320x240, RGB order.
  This changes the controller address mapping by 90 degrees while retaining
  the application's landscape layout. DDP, audio and sports features are identical.

## Install the test profile on COM5

Close the serial monitor first. From the repository on `feature/ddp-event-effects`:

```sh
git pull --ff-only
pio run -e cyd_ili9342 -t upload --upload-port COM5
pio device monitor --port COM5 --baud 115200
```

On first boot, tap and release the three crosshairs. The mapping handles swapped
or reversed touch axes and is saved separately for each display profile.
Each target times out after 20 seconds; timeout retains the previous/default
mapping. To retry, open the serial monitor, press reset, and send `C` when the
boot message requests it. No flash erase is needed; Wi-Fi settings are retained.
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
