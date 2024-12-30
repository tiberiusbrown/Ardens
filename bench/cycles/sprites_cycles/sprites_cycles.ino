#include <Arduboy2.h>
#include <ArduboyFX.h>

#include "SpritesABC.hpp"

#define SPRITESU_IMPLEMENTATION
#define SPRITESU_OVERWRITE
#define SPRITESU_PLUSMASK
#define SPRITESU_FX
#include "SpritesU.hpp"

#include "img_Sprites.hpp"
#include "img_SpritesU.hpp"
#include "fxdata.hpp"

#include <stdio.h>

Arduboy2Base a;

template<class F>
[[ gnu::noipa, gnu::noinline ]]
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

#if 1
    
    Serial.println(F("\nSprites::drawOverwrite"));
    debug_cycles([](){ Sprites::drawOverwrite(0, 0, IMG_SPRITESU, 0); });
    debug_cycles([](){ Sprites::drawOverwrite(0, 0, IMG_SPRITESU, 1); });
    debug_cycles([](){ Sprites::drawOverwrite(0, 4, IMG_SPRITESU, 0); });
    debug_cycles([](){ Sprites::drawOverwrite(-8, 0, IMG_SPRITESU, 0); });
    
    Serial.println(F("\nSpritesB::drawOverwrite"));
    debug_cycles([](){ SpritesB::drawOverwrite(0, 0, IMG_SPRITESU, 0); });
    debug_cycles([](){ SpritesB::drawOverwrite(0, 0, IMG_SPRITESU, 1); });
    debug_cycles([](){ SpritesB::drawOverwrite(0, 4, IMG_SPRITESU, 0); });
    debug_cycles([](){ SpritesB::drawOverwrite(-8, 0, IMG_SPRITESU, 0); });
    
    Serial.println(F("\nSpritesU::drawOverwrite"));
    debug_cycles([](){ SpritesU::drawOverwrite(0, 0, IMG_SPRITESU, 0); });
    debug_cycles([](){ SpritesU::drawOverwrite(0, 0, IMG_SPRITESU, 1); });
    debug_cycles([](){ SpritesU::drawOverwrite(0, 4, IMG_SPRITESU, 0); });
    debug_cycles([](){ SpritesU::drawOverwrite(-8, 0, IMG_SPRITESU, 0); });

    Serial.println(F("\nFX::drawBitmap (dbmOverwrite)"));
    debug_cycles([](){ FX::drawBitmap(0, 0, IMG_FX, 0, dbmOverwrite); });
    debug_cycles([](){ FX::drawBitmap(0, 0, IMG_FX, 1, dbmOverwrite); });
    debug_cycles([](){ FX::drawBitmap(0, 4, IMG_FX, 0, dbmOverwrite); });
    debug_cycles([](){ FX::drawBitmap(-8, 0, IMG_FX, 0, dbmOverwrite); });
    
    Serial.println(F("\nSpritesU::drawOverwriteFX"));
    debug_cycles([](){ SpritesU::drawOverwriteFX(0, 0, IMG_FX_SPRITESU, 0); });
    debug_cycles([](){ SpritesU::drawOverwriteFX(0, 0, IMG_FX_SPRITESU, 1); });
    debug_cycles([](){ SpritesU::drawOverwriteFX(0, 4, IMG_FX_SPRITESU, 0); });
    debug_cycles([](){ SpritesU::drawOverwriteFX(-8, 0, IMG_FX_SPRITESU, 0); });

    Serial.println(F("\nSpritesABC::drawBasic (MODE_OVERWRITE)"));
    debug_cycles([](){ SpritesABC::drawBasic(0, 0, 16, 16, IMG_FX_SPRITESU + 2, SpritesABC::MODE_OVERWRITE); });
    debug_cycles([](){ SpritesABC::drawBasic(0, 4, 16, 16, IMG_FX_SPRITESU + 2, SpritesABC::MODE_OVERWRITE); });
    debug_cycles([](){ SpritesABC::drawBasic(-8, 0, 16, 16, IMG_FX_SPRITESU + 2, SpritesABC::MODE_OVERWRITE); });

    Serial.println(F("\nSpritesABC::drawSized (MODE_OVERWRITE)"));
    debug_cycles([](){ SpritesABC::drawSized(0, 0, 16, 16, IMG_FX_SPRITESU + 2, SpritesABC::MODE_OVERWRITE, 0); });
    debug_cycles([](){ SpritesABC::drawSized(0, 0, 16, 16, IMG_FX_SPRITESU + 2, SpritesABC::MODE_OVERWRITE, 1); });
    debug_cycles([](){ SpritesABC::drawSized(0, 4, 16, 16, IMG_FX_SPRITESU + 2, SpritesABC::MODE_OVERWRITE, 0); });
    debug_cycles([](){ SpritesABC::drawSized(-8, 0, 16, 16, IMG_FX_SPRITESU + 2, SpritesABC::MODE_OVERWRITE, 0); });

    Serial.println(F("\nSpritesABC::draw (MODE_OVERWRITE)"));
    debug_cycles([](){ SpritesABC::draw(0, 0, IMG_FX_SPRITESU, SpritesABC::MODE_OVERWRITE, 0); });
    debug_cycles([](){ SpritesABC::draw(0, 0, IMG_FX_SPRITESU, SpritesABC::MODE_OVERWRITE, 1); });
    debug_cycles([](){ SpritesABC::draw(0, 4, IMG_FX_SPRITESU, SpritesABC::MODE_OVERWRITE, 0); });
    debug_cycles([](){ SpritesABC::draw(-8, 0, IMG_FX_SPRITESU, SpritesABC::MODE_OVERWRITE, 0); });
    
    Serial.println(F("\nSprites::drawPlusMask"));
    debug_cycles([](){ Sprites::drawPlusMask(0, 0, IMG_SPRITESU, 0); });
    debug_cycles([](){ Sprites::drawPlusMask(0, 0, IMG_SPRITESU, 1); });
    debug_cycles([](){ Sprites::drawPlusMask(0, 4, IMG_SPRITESU, 0); });
    debug_cycles([](){ Sprites::drawPlusMask(-8, 0, IMG_SPRITESU, 0); });
    
    Serial.println(F("\nSpritesB::drawPlusMask"));
    debug_cycles([](){ SpritesB::drawPlusMask(0, 0, IMG_SPRITESU, 0); });
    debug_cycles([](){ SpritesB::drawPlusMask(0, 0, IMG_SPRITESU, 1); });
    debug_cycles([](){ SpritesB::drawPlusMask(0, 4, IMG_SPRITESU, 0); });
    debug_cycles([](){ SpritesB::drawPlusMask(-8, 0, IMG_SPRITESU, 0); });
    
    Serial.println(F("\nSpritesU::drawPlusMask"));
    debug_cycles([](){ SpritesU::drawPlusMask(0, 0, IMG_SPRITESU, 0); });
    debug_cycles([](){ SpritesU::drawPlusMask(0, 0, IMG_SPRITESU, 1); });
    debug_cycles([](){ SpritesU::drawPlusMask(0, 4, IMG_SPRITESU, 0); });
    debug_cycles([](){ SpritesU::drawPlusMask(-8, 0, IMG_SPRITESU, 0); });

    Serial.println(F("\nFX::drawBitmap (dbmMasked)"));
    debug_cycles([](){ FX::drawBitmap(0, 0, IMG_FX, 0, dbmMasked); });
    debug_cycles([](){ FX::drawBitmap(0, 0, IMG_FX, 1, dbmMasked); });
    debug_cycles([](){ FX::drawBitmap(0, 4, IMG_FX, 0, dbmMasked); });
    debug_cycles([](){ FX::drawBitmap(-8, 0, IMG_FX, 0, dbmMasked); });    
    
    Serial.println(F("\nSpritesU::drawPlusMaskFX"));
    debug_cycles([](){ SpritesU::drawPlusMaskFX(0, 0, IMG_FX_SPRITESU, 0); });
    debug_cycles([](){ SpritesU::drawPlusMaskFX(0, 0, IMG_FX_SPRITESU, 1); });
    debug_cycles([](){ SpritesU::drawPlusMaskFX(0, 4, IMG_FX_SPRITESU, 0); });
    debug_cycles([](){ SpritesU::drawPlusMaskFX(-8, 0, IMG_FX_SPRITESU, 0); });

    Serial.println(F("\nSpritesABC::drawBasic (MODE_PLUSMASK)"));
    debug_cycles([](){ SpritesABC::drawBasic(0, 0, 16, 16, IMG_FX_SPRITESU + 2, SpritesABC::MODE_PLUSMASK); });
    debug_cycles([](){ SpritesABC::drawBasic(0, 4, 16, 16, IMG_FX_SPRITESU + 2, SpritesABC::MODE_PLUSMASK); });
    debug_cycles([](){ SpritesABC::drawBasic(-8, 0, 16, 16, IMG_FX_SPRITESU + 2, SpritesABC::MODE_PLUSMASK); });

    Serial.println(F("\nSpritesABC::drawSized (MODE_PLUSMASK)"));
    debug_cycles([](){ SpritesABC::drawSized(0, 0, 16, 16, IMG_FX_SPRITESU + 2, SpritesABC::MODE_PLUSMASK, 0); });
    debug_cycles([](){ SpritesABC::drawSized(0, 0, 16, 16, IMG_FX_SPRITESU + 2, SpritesABC::MODE_PLUSMASK, 1); });
    debug_cycles([](){ SpritesABC::drawSized(0, 4, 16, 16, IMG_FX_SPRITESU + 2, SpritesABC::MODE_PLUSMASK, 0); });
    debug_cycles([](){ SpritesABC::drawSized(-8, 0, 16, 16, IMG_FX_SPRITESU + 2, SpritesABC::MODE_PLUSMASK, 0); });

    Serial.println(F("\nSpritesABC::draw (MODE_PLUSMASK)"));
    debug_cycles([](){ SpritesABC::draw(0, 0, IMG_FX_SPRITESU, SpritesABC::MODE_PLUSMASK, 0); });
    debug_cycles([](){ SpritesABC::draw(0, 0, IMG_FX_SPRITESU, SpritesABC::MODE_PLUSMASK, 1); });
    debug_cycles([](){ SpritesABC::draw(0, 4, IMG_FX_SPRITESU, SpritesABC::MODE_PLUSMASK, 0); });
    debug_cycles([](){ SpritesABC::draw(-8, 0, IMG_FX_SPRITESU, SpritesABC::MODE_PLUSMASK, 0); });

#endif
}

void loop() {
    if(!a.nextFrame())
        return;

    SpritesABC::draw(0, 0, IMG_FX_SPRITESU, SpritesABC::MODE_OVERWRITE, 0);
    SpritesABC::draw(32, 4, IMG_FX_SPRITESU, SpritesABC::MODE_PLUSMASK, 0);
    SpritesABC::drawSized(64, 2, 16, 16, IMG_FX_SPRITESU + 2, SpritesABC::MODE_PLUSMASK, 0);
    
    FX::display(CLEAR_BUFFER);
}
