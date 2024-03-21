# Ardens

Ardens is an [Arduboy](https://www.arduboy.com/) simulator that is useful for profiling and debugging.

![screenshot](img/screenshot.png)

[Try out the full debugger here.](https://tiberiusbrown.github.io/Ardens/)
Drag and drop an [Arduboy game](https://community.arduboy.com/c/games/35) (.hex/.elf/.arduboy) and any FX data (.bin) onto the page.

Try the minimal player with one of the game URLs below:

- [ArduChess](https://tiberiusbrown.github.io/Ardens/player.html?file=https://raw.githubusercontent.com/tiberiusbrown/arduchess/master/arduchess/arduchess.hex)
- [ArduGolf](https://tiberiusbrown.github.io/Ardens/player.html?file=https://raw.githubusercontent.com/tiberiusbrown/arduboy_minigolf/master/ardugolf.hex)
- [ArduRogue](https://tiberiusbrown.github.io/Ardens/player.html?file=https://raw.githubusercontent.com/tiberiusbrown/ardurogue/master/ardurogue.hex)

## Features

- Profiler
  - Identify performance hotspots at the instruction level
  - View inclusive CPU load or raw cycle counts per instruction, hotspot, or symbol
- Live CPU Usage Graph
- Disassembly View
  - Source lines, symbols, and labels intermixed with instructions
  - Addresses of jumps, branches, and calls expanded to label/offset
  - Click on jump addresses to scroll to their destination
  - Hover over RAM addresses to see their symbol, data type, and value
  - Jump to any symbol from sorted list
- Source View
- Call Stack
- RAM/Registers View
  - Edit any register or RAM value live
  - Show RAM consumption from global variables
  - Track current and maximum stack usage
  - Set read/write breakpoints on RAM address
  - View a portion of RAM as a display buffer
- Simulation Control
  - Slow down or speed up simulation speed
  - Pause, continue, step into, step over, step out
- Serial Monitor
- LEDs
- EEPROM (editable)
- External flash (editable)
- Display (SSD1306) Internals
- Display View
  - Customize palette and filters
  - Optionally model row driver current limiting
  - Temporal filtering for grayscale games
- Sound Waveform (Time and Frequency)
- Global and Local Variables
  - Show all accessible locals and their data types and values
  - User-defined list of globals to watch
- Screenshots
  - Record PNG or GIF
  - Optionally record sound as WAV
  - Customize palettes and filters for recording
- Snapshots
  - Save/restore snapshot of entire device state

## Keyboard Bindings

|   Key  | Purpose                            |
|:------:|------------------------------------|
| Arrows | Arduboy: directional buttons       |
|   A/Z  | Arduboy: A button                  |
|  S/X/B | Arduboy: B button                  |
|   F1   | (Desktop Debugger Only) Save screenshot of entire window |
|   F2   | Save screenshot                    |
|   F3   | Toggle GIF recording               |
|   F4   | (Debugger only) Save snapshot      |
|   F5   | (Debugger only) Pause/Continue     |
|   F8   | Reset                              |
|   F11  | Toggle fullscreen                  |
|    O   | (Debugger only) Settings window    |
|    P   | (Debugger only) Toggle player mode |
|    R   | Rotate display                     |

## Command Line Usage

To invoke from the command line with specific options and game file(s), use the syntax `/path/to/Ardens param=value file=game.hex`, where the parameters are the same as in the URL parameters below.

For example:

```raw
Ardens palette=highcontrast grid=normal current=true file=game.hex file=fxdata.bin
```

## Creating a link to a playable Arduboy game

There is a web build hosted on GitHub pages: to create a link to a playable Arduboy game using Ardens Player, you can follow these patterns:

`https://tiberiusbrown.github.io/Ardens/player.html?file=https://example.com/game.arduboy`
`https://tiberiusbrown.github.io/Ardens/player.html?file=https://example.com/game.hex&file=https://example.com/game.bin`

The web builds of the debugger and player accept the following additional URL parameters (e.g., add `&param1=value1&param2=value2` to the URL).

#### `g` or `grid`
Overlays the display with a grid to allow easier disambiguation of adjacent pixels.
Values: `none`, `normal`, `red`, `green`, `blue`, `cyan`, `magenta`, `yellow`, `white`

#### `p` or `palette`
Adjusts the color palette of the display.
Values: `default`, `retro`, `lowcontrast`, `highcontrast`

#### `af` or `autofilter`
If enabled, applies temporal filtering to the display to help properly render grayscale games.
Values: `0`, `1`

#### `f` or `filter`
(Debugger only) Conditionally applies an upscaling filter.
Values: `none`, `scale2x`, `scale3x`, `scale4x`, `hq2x`, `hq3x`, `hq4x`

#### `ds` or `downsample`
(Debugger only) Downsamples by an integer ratio after applying an upsample filter.
Values: `0`, `1`, `2`, `3`

#### `ori` or `orientation`
Rotates the display in 90 degree increments (directional button mapping is also rotated as if playing on a rotated Arduboy).
Values:
- `0` or `normal`
- `90` or `cw` or `cw90`
- `180` or `flip`
- `270` or `ccw` or `ccw90`

#### `v` or `volume`
Sets the emulated audio gain. Value may be in the range 0 to 200, with 100 being the default.

#### `size` (Desktop only)
Sets the initial size of the application window. Value should be in the format `<width>x<height>` (for example, `size=800x400`).

#### `i` or `intscale`
Restricts the display scaling to an integral scaling factor. Defaults to false for the web player, and true for all other platforms.

#### `c` or `current`
Enables modeling the display row driver current limit. This effectively darkens rows that have many lit pixels. Defaults to false.

#### `fxport`
Sets the port to be used for the flash select. Values:
- `0` or `d1` or `fx` (default)
- `1` or `d2` or `fxdevkit`
- `2` or `e2` or `mini`

#### `display`
Sets the type of emulated display. Values:
- `0` or `ssd1306` (default)
- `1` or `ssd1309`
- `2` or `sh1106`

## Libraries Used

- [SDL](https://github.com/libsdl-org/SDL) (zlib)
- [sokol](https://github.com/floooh/sokol) (zlib)
- [LLVM Project](https://github.com/llvm/llvm-project) (Apache v2.0 with LLVM Exceptions) 
- [Dear ImGui](https://github.com/ocornut/imgui) (MIT)
- [ImPlot](https://github.com/epezent/implot) (MIT)
- [imgui_markdown](https://github.com/juliettef/imgui_markdown) (zlib)
- [ImGuiColorTextEdit](https://github.com/BalazsJako/ImGuiColorTextEdit) (MIT)
- [Simple-FFT](https://github.com/d1vanov/Simple-FFT) (MIT)
- [brunexgeek/hqx](https://github.com/brunexgeek/hqx) (Apache v2.0)
- [{fmt}](https://github.com/fmtlib/fmt) (MIT)
- [rapidjson](https://github.com/Tencent/rapidjson) (MIT)
- [Miniz](https://github.com/richgel999/miniz) (MIT)
- [Bitsery](https://github.com/fraillt/bitsery) (MIT)
- [stb_image_write](https://github.com/nothings/stb/blob/master/stb_image_write.h) (Public Domain)
- [GIF encoder](https://github.com/lecram/gifenc) (Public Domain)
- [AudioFile](https://github.com/adamstark/AudioFile) (MIT)
- [Emscripten Browser File Library](https://github.com/Armchair-Software/emscripten-browser-file) (MIT)
- [Filewatch](https://github.com/ThomasMonkman/filewatch) (MIT)
