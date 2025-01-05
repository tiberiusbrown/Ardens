#include <Arduboy2.h>
#include <ArduboyFX.h>

#include "SpritesABC.hpp"

#define SPRITESU_IMPLEMENTATION
#define SPRITESU_OVERWRITE
#define SPRITESU_PLUSMASK
#define SPRITESU_FX
#include "SpritesU.hpp"

#include "img_SpritesU.hpp"
#include "fxdata.hpp"

#include <stdio.h>

Arduboy2Base a;

template<class F>
void debug_cycles(F&& f)
{
    uint8_t sreg = SREG;
    cli();
    asm volatile("break\n");
    f();
    asm volatile("break\n");
    SREG = sreg;
}

int debug_putc(char c, FILE* f) 
{
    UEDATX = c;
    return c;
}

void setup() {
    a.boot();
    FX::begin(FX_ADDR);

    Serial.begin(9600);
    fdevopen(&debug_putc, 0);

#if 0
#else
    uint8_t ws[] = { 2, 8, 16, 32, 64, 128 };
    uint8_t hs[] = { 8, 8, 16, 32, 64,  64 };

    for(uint8_t i = 0; i < 6; ++i)
    {
        
        int16_t x = -ws[i]/2;
        
        Serial.print(F("\n\n### Dimensions: "));
        Serial.print((int)ws[i]);
        Serial.print('x');
        Serial.print((int)hs[i]);
        Serial.print(F("\n\n| Method | (0, 0) f0 | (0, 0) f1 | (0, 4) f0 | ("));
        Serial.print(x);
        Serial.print(F(", 0) f0 |"));
        Serial.print(F("\n|---|:-:|:-:|:-:|:-:|"));

        Serial.print(F("\n| <td colspan=2>*Unmasked*</td> |"));

        Serial.print(F("\n| Sprites::drawOverwrite |"));
        debug_cycles([=](){ Sprites::drawOverwrite(0, 0, IMG_SPRITESU + i * 2, 0); });
        debug_cycles([=](){ Sprites::drawOverwrite(0, 0, IMG_SPRITESU + i * 2, 1); });
        debug_cycles([=](){ Sprites::drawOverwrite(0, 4, IMG_SPRITESU + i * 2, 0); });
        debug_cycles([=](){ Sprites::drawOverwrite(x, 0, IMG_SPRITESU + i * 2, 0); });
        
        Serial.print(F("\n| SpritesB::drawOverwrite |"));
        debug_cycles([=](){ SpritesB::drawOverwrite(0, 0, IMG_SPRITESU + i * 2, 0); });
        debug_cycles([=](){ SpritesB::drawOverwrite(0, 0, IMG_SPRITESU + i * 2, 1); });
        debug_cycles([=](){ SpritesB::drawOverwrite(0, 4, IMG_SPRITESU + i * 2, 0); });
        debug_cycles([=](){ SpritesB::drawOverwrite(x, 0, IMG_SPRITESU + i * 2, 0); });
        
        Serial.print(F("\n| SpritesU::drawOverwrite |"));
        debug_cycles([=](){ SpritesU::drawOverwrite(0, 0, IMG_SPRITESU + i * 2, 0); });
        debug_cycles([=](){ SpritesU::drawOverwrite(0, 0, IMG_SPRITESU + i * 2, 1); });
        debug_cycles([=](){ SpritesU::drawOverwrite(0, 4, IMG_SPRITESU + i * 2, 0); });
        debug_cycles([=](){ SpritesU::drawOverwrite(x, 0, IMG_SPRITESU + i * 2, 0); });

        Serial.print(F("\n| FX::drawBitmap (dbmOverwrite) |"));
        debug_cycles([=](){ FX::drawBitmap(0, 0, IMG_FX + i * 4, 0, dbmOverwrite); });
        debug_cycles([=](){ FX::drawBitmap(0, 0, IMG_FX + i * 4, 1, dbmOverwrite); });
        debug_cycles([=](){ FX::drawBitmap(0, 4, IMG_FX + i * 4, 0, dbmOverwrite); });
        debug_cycles([=](){ FX::drawBitmap(x, 0, IMG_FX + i * 4, 0, dbmOverwrite); });
        
        Serial.print(F("\n| SpritesU::drawOverwriteFX |"));
        debug_cycles([=](){ SpritesU::drawOverwriteFX(0, 0, IMG_FX_SPRITESU + i * 2, 0); });
        debug_cycles([=](){ SpritesU::drawOverwriteFX(0, 0, IMG_FX_SPRITESU + i * 2, 1); });
        debug_cycles([=](){ SpritesU::drawOverwriteFX(0, 4, IMG_FX_SPRITESU + i * 2, 0); });
        debug_cycles([=](){ SpritesU::drawOverwriteFX(x, 0, IMG_FX_SPRITESU + i * 2, 0); });

        Serial.print(F("\n| SpritesABC::drawFX (MODE_OVERWRITE) |"));
        debug_cycles([=](){ SpritesABC::drawFX(0, 0, IMG_FX_SPRITESU + i * 2, SpritesABC::MODE_OVERWRITE, 0); });
        debug_cycles([=](){ SpritesABC::drawFX(0, 0, IMG_FX_SPRITESU + i * 2, SpritesABC::MODE_OVERWRITE, 1); });
        debug_cycles([=](){ SpritesABC::drawFX(0, 4, IMG_FX_SPRITESU + i * 2, SpritesABC::MODE_OVERWRITE, 0); });
        debug_cycles([=](){ SpritesABC::drawFX(x, 0, IMG_FX_SPRITESU + i * 2, SpritesABC::MODE_OVERWRITE, 0); });

        Serial.print(F("\n| SpritesABC::drawSizedFX (MODE_OVERWRITE) |"));
        debug_cycles([=](){ SpritesABC::drawSizedFX(0, 0, ws[i], hs[i], IMG_FX_SPRITESU + 2, SpritesABC::MODE_OVERWRITE, 0); });
        debug_cycles([=](){ SpritesABC::drawSizedFX(0, 0, ws[i], hs[i], IMG_FX_SPRITESU + 2, SpritesABC::MODE_OVERWRITE, 1); });
        debug_cycles([=](){ SpritesABC::drawSizedFX(0, 4, ws[i], hs[i], IMG_FX_SPRITESU + 2, SpritesABC::MODE_OVERWRITE, 0); });
        debug_cycles([=](){ SpritesABC::drawSizedFX(x, 0, ws[i], hs[i], IMG_FX_SPRITESU + 2, SpritesABC::MODE_OVERWRITE, 0); });

        Serial.print(F("\n| SpritesABC::drawBasicFX (MODE_OVERWRITE) |"));
        debug_cycles([=](){ SpritesABC::drawBasicFX(0, 0, ws[i], hs[i], IMG_FX_SPRITESU + 2, SpritesABC::MODE_OVERWRITE); });
        Serial.print(F(" - |"));
        debug_cycles([=](){ SpritesABC::drawBasicFX(0, 4, ws[i], hs[i], IMG_FX_SPRITESU + 2, SpritesABC::MODE_OVERWRITE); });
        debug_cycles([=](){ SpritesABC::drawBasicFX(x, 0, ws[i], hs[i], IMG_FX_SPRITESU + 2, SpritesABC::MODE_OVERWRITE); });
        
        Serial.print(F("\n| <td colspan=2>*Masked*</td> |"));
        
        Serial.print(F("\n| Sprites::drawPlusMask |"));
        debug_cycles([=](){ Sprites::drawPlusMask(0, 0, IMG_SPRITESU + i * 2, 0); });
        debug_cycles([=](){ Sprites::drawPlusMask(0, 0, IMG_SPRITESU + i * 2, 1); });
        debug_cycles([=](){ Sprites::drawPlusMask(0, 4, IMG_SPRITESU + i * 2, 0); });
        debug_cycles([=](){ Sprites::drawPlusMask(x, 0, IMG_SPRITESU + i * 2, 0); });
        
        Serial.print(F("\n| SpritesB::drawPlusMask |"));
        debug_cycles([=](){ SpritesB::drawPlusMask(0, 0, IMG_SPRITESU + i * 2, 0); });
        debug_cycles([=](){ SpritesB::drawPlusMask(0, 0, IMG_SPRITESU + i * 2, 1); });
        debug_cycles([=](){ SpritesB::drawPlusMask(0, 4, IMG_SPRITESU + i * 2, 0); });
        debug_cycles([=](){ SpritesB::drawPlusMask(x, 0, IMG_SPRITESU + i * 2, 0); });
        
        Serial.print(F("\n| SpritesU::drawPlusMask |"));
        debug_cycles([=](){ SpritesU::drawPlusMask(0, 0, IMG_SPRITESU + i * 2, 0); });
        debug_cycles([=](){ SpritesU::drawPlusMask(0, 0, IMG_SPRITESU + i * 2, 1); });
        debug_cycles([=](){ SpritesU::drawPlusMask(0, 4, IMG_SPRITESU + i * 2, 0); });
        debug_cycles([=](){ SpritesU::drawPlusMask(x, 0, IMG_SPRITESU + i * 2, 0); });

        Serial.print(F("\n| FX::drawBitmap (dbmMasked) |"));
        debug_cycles([=](){ FX::drawBitmap(0, 0, IMG_FX + i * 4, 0, dbmMasked); });
        debug_cycles([=](){ FX::drawBitmap(0, 0, IMG_FX + i * 4, 1, dbmMasked); });
        debug_cycles([=](){ FX::drawBitmap(0, 4, IMG_FX + i * 4, 0, dbmMasked); });
        debug_cycles([=](){ FX::drawBitmap(x, 0, IMG_FX + i * 4, 0, dbmMasked); });    
        
        Serial.print(F("\n| SpritesU::drawPlusMaskFX |"));
        debug_cycles([=](){ SpritesU::drawPlusMaskFX(0, 0, IMG_FX_SPRITESU + i * 2, 0); });
        debug_cycles([=](){ SpritesU::drawPlusMaskFX(0, 0, IMG_FX_SPRITESU + i * 2, 1); });
        debug_cycles([=](){ SpritesU::drawPlusMaskFX(0, 4, IMG_FX_SPRITESU + i * 2, 0); });
        debug_cycles([=](){ SpritesU::drawPlusMaskFX(x, 0, IMG_FX_SPRITESU + i * 2, 0); });

        Serial.print(F("\n| SpritesABC::drawFX (MODE_PLUSMASK) |"));
        debug_cycles([=](){ SpritesABC::drawFX(0, 0, IMG_FX_SPRITESU + i * 2, SpritesABC::MODE_PLUSMASK, 0); });
        debug_cycles([=](){ SpritesABC::drawFX(0, 0, IMG_FX_SPRITESU + i * 2, SpritesABC::MODE_PLUSMASK, 1); });
        debug_cycles([=](){ SpritesABC::drawFX(0, 4, IMG_FX_SPRITESU + i * 2, SpritesABC::MODE_PLUSMASK, 0); });
        debug_cycles([=](){ SpritesABC::drawFX(x, 0, IMG_FX_SPRITESU + i * 2, SpritesABC::MODE_PLUSMASK, 0); });

        Serial.print(F("\n| SpritesABC::drawSizedFX (MODE_PLUSMASK) |"));
        debug_cycles([=](){ SpritesABC::drawSizedFX(0, 0, ws[i], hs[i], IMG_FX_SPRITESU + 2, SpritesABC::MODE_PLUSMASK, 0); });
        debug_cycles([=](){ SpritesABC::drawSizedFX(0, 0, ws[i], hs[i], IMG_FX_SPRITESU + 2, SpritesABC::MODE_PLUSMASK, 1); });
        debug_cycles([=](){ SpritesABC::drawSizedFX(0, 4, ws[i], hs[i], IMG_FX_SPRITESU + 2, SpritesABC::MODE_PLUSMASK, 0); });
        debug_cycles([=](){ SpritesABC::drawSizedFX(x, 0, ws[i], hs[i], IMG_FX_SPRITESU + 2, SpritesABC::MODE_PLUSMASK, 0); });

        Serial.print(F("\n| SpritesABC::drawBasicFX (MODE_PLUSMASK) |"));
        debug_cycles([=](){ SpritesABC::drawBasicFX(0, 0, ws[i], hs[i], IMG_FX_SPRITESU + 2, SpritesABC::MODE_PLUSMASK); });
        Serial.print(F(" - |"));
        debug_cycles([=](){ SpritesABC::drawBasicFX(0, 4, ws[i], hs[i], IMG_FX_SPRITESU + 2, SpritesABC::MODE_PLUSMASK); });
        debug_cycles([=](){ SpritesABC::drawBasicFX(x, 0, ws[i], hs[i], IMG_FX_SPRITESU + 2, SpritesABC::MODE_PLUSMASK); });
    
    }

    Serial.print(F("\n\n"));
#endif
}

void loop() {
    if(!a.nextFrame())
        return;
    
    FX::display(CLEAR_BUFFER);
}
