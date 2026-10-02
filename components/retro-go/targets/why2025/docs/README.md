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
- Audio: MAX98357A with 16-bit standard I²S, BCLK GPIO26, LRCLK GPIO25,
  DIN GPIO27, and active-high amplifier enable GPIO24. No MCLK is needed.
  Retro-Go mixes stereo to mono in software and retains its volume/mute controls.
  A streaming linear interpolator converts emulator audio to a fixed 48 kHz
  output, including 22.05 kHz and other native rates unsupported by MAX98357A.
  Six 5 ms DMA buffers provide 30 ms of output buffering (25 ms writable while
  one buffer plays), covering a 50 Hz frame plus scheduling/rendering jitter.
  Consumed DMA buffers are cleared so an underrun cannot replay stale samples.
  The amplifier stays shut down during initialization and after audio teardown.

## Speaker audio

Connect a suitable 4 Ω or 8 Ω speaker across the two **LS1** pads on the carrier.
These are differential speaker outputs; neither pad is ground. Start at low
volume. The MAX98357A is the amplifier, so a speaker must still be attached.

Audio defaults to the **Ext DAC** sink on a fresh configuration. If your existing
settings select **Dummy**, change the audio output to **Ext DAC** in the options.
Audible playback, mute/unmute, volume, and switching games/sample rates still need
validation on a physical badge.

Host regression checks: `python3 tools/tests/test_i2s_submit.py`. These cover
submission boundaries, stereo/mono, volume/mute, failed writes, output sample
counts and interpolation continuity across submissions at the emulator rates,
plus the fixed-rate DMA time budget and underrun clearing configuration.

The production M.2 and carrier net names differ. The mapping below follows the
connector pad numbers in the official hardware release
[`v2.5-corrected-SD-detection`](https://gitlab.com/why2025/team-badge/Hardware/-/tree/v2.5-corrected-SD-detection):

| ESP32-P4 GPIO | M.2 net name | Connector pad | Carrier signal |
| --- | --- | --- | --- |
| 24 | I2S.DATA | 2 | SD_MODE (enable) |
| 27 | I2S.LRCK | 4 | DIN |
| 26 | I2S.MCLK | 6 | BCLK |
| 25 | I2S.SCLK | 8 | LRCLK |

GPIO24 high selects the MAX98357A's left channel; both transmitted slots contain
the averaged stereo signal so audio from either game channel is preserved.
The carrier has a 10 kΩ pull-down on SD_MODE and a 100 kΩ gain resistor to ground.

## Controls

| Badge key | Retro-Go action |
| --- | --- |
| W / A / S / D | D-pad up / left / down / right |
| Circle / Cross | A / B |
| Triangle / Square | X / Y |
| Cloud / Diamond | L / R |
| Return / Backspace | Start / Select |
| Escape | Menu and boot recovery |
| Tab | Options |

## Limitations

Battery measurement, brightness control, and ESP32-C6 networking are not
implemented. The supplied BadgeVMS drivers provide no battery implementation
or backlight GPIO to reuse. Network update support is disabled.

## Source provenance

- Official WHY2025 hardware release linked above: `M2/badgeM2Card.kicad_pcb`
  and `Carrier/badgeCarrierCard.kicad_pcb` for the end-to-end audio pin mapping.
  [MAX98357A datasheet](https://www.analog.com/media/en/technical-documentation/data-sheets/MAX98357A-MAX98357B.pdf)
  for I²S format, channel selection and speaker output requirements.
- `firmware/badgevms/drivers/st7703.c` and `st7703.h`: panel wiring, power,
  timing and supplier initialization commands. Adapted tables in `panel.h` retain
  their BadgeVMS GPL-3.0-or-later notice; see `COPYING.BadgeVMS`.
- `firmware/components/esp_lcd_st7703`: vendored Espressif 1.0.3 panel driver
  (including BadgeVMS changes), Apache-2.0. Its local CMake integration replaces
  the package-manager version helper; source and header are preserved.
- `firmware/badgevms/drivers/fatfs.c`: SDMMC wiring and power.
- `firmware/badgevms/drivers/tca8418.c` and `components/esp_tca8418`: keyboard
  scancodes and register configuration.
