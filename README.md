# arduboy_sim

[Arduboy](https://www.arduboy.com/) simulator for profiling and debugging.

![screenshot](img/screenshot.png)

[Try out the full debugger here.](https://tiberiusbrown.github.io/arduboy_sim/)
Drag and drop an [Arduboy game](https://community.arduboy.com/c/games/35) (.hex/.elf/.arduboy) and any FX data (.bin) onto the page.

Try the minimal player with one of the game URLs below:

- [ArduChess](https://tiberiusbrown.github.io/arduboy_sim/player.html?file=https://raw.githubusercontent.com/tiberiusbrown/arduchess/master/arduchess/arduchess.hex)
- [ArduGolf](https://tiberiusbrown.github.io/arduboy_sim/player.html?file=https://raw.githubusercontent.com/tiberiusbrown/arduboy_minigolf/master/ardugolf.hex)
- [ArduRogue](https://tiberiusbrown.github.io/arduboy_sim/player.html?file=https://raw.githubusercontent.com/tiberiusbrown/ardurogue/master/ardurogue.hex)

## URL parameters

The web builds of the debugger and player accept the following URL parameters (e.g., add `?param1=value1&param2=value2` to the URL):

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
- [JSON for Modern C++](https://github.com/nlohmann/json) (MIT)
- [Miniz](https://github.com/richgel999/miniz) (MIT)
- [Bitsery](https://github.com/fraillt/bitsery) (MIT)
- [stb_image_write](https://github.com/nothings/stb/blob/master/stb_image_write.h) (Public Domain)
- [GIF encoder](https://github.com/lecram/gifenc) (Public Domain)
- [AudioFile](https://github.com/adamstark/AudioFile) (MIT)
- [Emscripten Browser File Library](https://github.com/Armchair-Software/emscripten-browser-file) (MIT)
