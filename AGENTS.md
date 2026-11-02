# Firmware configuration

- Keep controller-specific ESP32 firmware in separate PlatformIO environments and distinctly named CI artifacts; board revisions vary, so an alternative must not replace the existing ILI9341 default.
- Define native panel dimensions in portrait order and rotate display rendering at runtime; the GameDay UI and touch coordinate space are 320x240 landscape.
