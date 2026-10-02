# WHY2025 badge

Initial Retro-Go port for the ESP32-P4 WHY2025 badge, using the local BadgeVMS
firmware as the hardware reference. Default panel: **v2.1 Bono (black border)**.
Hardware validation is still required.

## Build

Use ESP-IDF 5.5 with its ESP32-P4 toolchain and an exported IDF environment.
From the Retro-Go root:

```sh
python rg_tool.py --target why2025 build-img --no-networking
```

The target retains the GB300-P4 base configuration for 16 MB flash and 200 MHz
PSRAM, with PSRAM initialized by ESP-IDF and available to the allocator. Do not
copy BadgeVMS's disabled PSRAM boot initialization: BadgeVMS initializes its own
memory system, whereas Retro-Go and the DPI driver need the IDF heap.

## Hardware implementation

- ST7703: 720×720 RGB565, little-endian pixels, BGR panel order, reset GPIO17
  active high. Two DSI lanes at 1000 Mbps; PHY LDO channel 3 at 2500 mV.
- Bono timing: 47 MHz pixel clock, horizontal sync/back/front porch 60/120/106,
  vertical sync/back/front porch 4/20/20. Supplier command tables follow BadgeVMS.
- Mountain panels (blue border): change `RG_WHY2025_PANEL_MOUNTAIN` in `config.h`
  to `1`. This selects the separate supplier table and 58 MHz timings.
- The display is rotated **90 degrees counterclockwise** using the ESP32-P4 PPA.
  `RG_SCREEN_ROTATION` in `config.h` accepts `90` (default) or `0` (unrotated).
  These are degrees for this backend, not the SPI drivers' MADCTL bit values.
  Menus and games share the rotation; keyboard mappings are unchanged.
- Updates accumulate in an RGB565 shadow image. At display synchronization, PPA
  rotates the dirty bounding rectangle into a separate packed buffer, then a
  synchronous DPI copy presents it. Partial windows, gaps between updates, and
  chunks ending inside a row are supported. Two cache-aligned PSRAM buffers add
  2,073,600 bytes (about 1.98 MiB); neither is allocated with rotation disabled.
  PPA handles cache synchronization for rotation, and DPI handles scanout cache
  writeback. Single scanout buffering can still cause visible tearing.
  Rotation throughput and visual orientation require validation on hardware.
- SD card: SDMMC slot 0, four bits, CLK43/CMD44/D0–D3=39–42, power LDO channel 4.
- Keyboard: TCA8418 at I2C address 0x34, SDA18/SCL20, eight rows and ten columns.
  Polling preserves held keys and simultaneous presses. FIFO overflow or I2C read
  failure clears held state; release and press keys again after recovery.

## Controls

| Badge key | Retro-Go action |
| --- | --- |
| Arrow keys | D-pad |
| Circle / Cross | A / B |
| Triangle / Square | X / Y |
| Cloud / Diamond | L / R |
| Return / Backspace | Start / Select |
| Escape | Menu and boot recovery |
| Tab | Options |

## Limitations

Audio, battery measurement, brightness control, and ESP32-C6 networking are not
implemented. The supplied BadgeVMS drivers provide no audio/battery implementation
or backlight GPIO to reuse. Audio uses Retro-Go's timed dummy sink. Network update
support is disabled.

## Source provenance

- `firmware/badgevms/drivers/st7703.c` and `st7703.h`: panel wiring, power,
  timing and supplier initialization commands. Adapted tables in `panel.h` retain
  their BadgeVMS GPL-3.0-or-later notice; see `COPYING.BadgeVMS`.
- `firmware/components/esp_lcd_st7703`: vendored Espressif 1.0.3 panel driver
  (including BadgeVMS changes), Apache-2.0. Its local CMake integration replaces
  the package-manager version helper; source and header are preserved.
- `firmware/badgevms/drivers/fatfs.c`: SDMMC wiring and power.
- `firmware/badgevms/drivers/tca8418.c` and `components/esp_tca8418`: keyboard
  scancodes and register configuration.
