// SpritesABC by Peter Brown
// performance-optimized FX sprites rendering

#pragma once

#include <Arduboy2.h>
#include <ArduboyFX.h>

struct SpritesABC
{
    // color: 0 for BLACK, 1 for WHITE
    static void fillRect(int16_t x, int16_t y, uint8_t w, uint8_t h, uint8_t color);

    static constexpr uint8_t MODE_OVERWRITE      = 0;
    static constexpr uint8_t MODE_PLUSMASK       = 1;
    static constexpr uint8_t MODE_SELFMASK       = 4;
    static constexpr uint8_t MODE_SELFMASK_ERASE = 6; // like selfmask but erases pixels

    static void drawBasicFX(
        int16_t x, int16_t y, uint8_t w, uint8_t h,
        uint24_t image, uint8_t mode);
    
    static void drawSizedFX(
        int16_t x, int16_t y, uint8_t w, uint8_t h,
        uint24_t image, uint8_t mode, uint16_t frame);
    
    static void drawFX(
        int16_t x, int16_t y,
        uint24_t image, uint8_t mode, uint16_t frame);
};
