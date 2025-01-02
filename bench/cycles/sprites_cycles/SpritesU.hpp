/*
Options

    SPRITESU_IMPLEMENTATION
    SPRITESU_OVERWRITE
    SPRITESU_PLUSMASK
    SPRITESU_FX
    SPRITESU_RECT
*/

#pragma once

#if defined(SPRITESU_FX)
#include <ArduboyFX.h>
#else
using uint24_t = __uint24;
#endif

struct SpritesU
{
#ifdef SPRITESU_OVERWRITE
    static void drawOverwrite(
        int16_t x, int16_t y, uint8_t const* image, uint16_t frame);
    static void drawOverwrite(
        int16_t x, int16_t y, uint8_t w, uint8_t h, uint8_t const* image);
#endif

#ifdef SPRITESU_PLUSMASK
    static void drawPlusMask(
        int16_t x, int16_t y, uint8_t const* image, uint16_t frame);
    static void drawPlusMask(
        int16_t x, int16_t y, uint8_t w, uint8_t h, uint8_t const* image);
#endif

#if defined(SPRITESU_OVERWRITE) || defined(SPRITESU_PLUSMASK)
    static void drawSelfMask(
        int16_t x, int16_t y, uint8_t const* image, uint16_t frame);
    static void drawSelfMask(
        int16_t x, int16_t y, uint8_t w, uint8_t h, uint8_t const* image);
#endif

#ifdef SPRITESU_FX
    static void drawOverwriteFX(
        int16_t x, int16_t y, uint24_t image, uint16_t frame);
    static void drawOverwriteFX(
        int16_t x, int16_t y, uint8_t w, uint8_t h, uint24_t image, uint16_t frame);
    static void drawPlusMaskFX(
        int16_t x, int16_t y, uint24_t image, uint16_t frame);
    static void drawPlusMaskFX(
        int16_t x, int16_t y, uint8_t w, uint8_t h, uint24_t image, uint16_t frame);
    static void drawSelfMaskFX(
        int16_t x, int16_t y, uint24_t image, uint16_t frame);
    static void drawSelfMaskFX(
        int16_t x, int16_t y, uint8_t w, uint8_t h, uint24_t image, uint16_t frame);
#endif

#ifdef SPRITESU_RECT
    // color: zero for BLACK, 1 for WHITE
    static void fillRect(int16_t x, int16_t y, uint8_t w, uint8_t h, uint8_t color);
    static void fillRect_i8(int8_t x, int8_t y, uint8_t w, uint8_t h, uint8_t color);
#endif

    static constexpr uint8_t MODE_OVERWRITE   = 0;
    static constexpr uint8_t MODE_PLUSMASK    = 1;
    static constexpr uint8_t MODE_SELFMASK    = 4;
    static constexpr uint8_t MODE_OVERWRITEFX = 2;
    static constexpr uint8_t MODE_PLUSMASKFX  = 3;
    static constexpr uint8_t MODE_SELFMASKFX  = 6;

    static void drawBasic(
        int16_t x, int16_t y, uint8_t w, uint8_t h,
        uint24_t image, uint16_t frame, uint8_t mode);
    static void drawBasicNoChecks(
        uint16_t w_and_h,
        uint24_t image, uint8_t mode,
        int16_t x, int16_t y);
};

#ifdef SPRITESU_IMPLEMENTATION

// from Mr. Blinky's ArduboyFX library
static __attribute__((always_inline)) uint8_t SpritesU_bitShiftLeftUInt8(uint8_t bit)
{
#ifdef ARDUINO_ARCH_AVR
    uint8_t result;
    asm volatile(
        "ldi    %[result], 1    \n" // 0 = 000 => 0000 0001
        "sbrc   %[bit], 1       \n" // 1 = 001 => 0000 0010
        "ldi    %[result], 4    \n" // 2 = 010 => 0000 0100
        "sbrc   %[bit], 0       \n" // 3 = 011 => 0000 1000
        "lsl    %[result]       \n"
        "sbrc   %[bit], 2       \n" // 4 = 100 => 0001 0000
        "swap   %[result]       \n" // 5 = 101 => 0010 0000
        :[result] "=&d" (result)    // 6 = 110 => 0100 0000
        :[bit]    "r"   (bit)       // 7 = 111 => 1000 0000
        :
    );
    return result;
#else
    return 1 << (bit & 7);
#endif
}

void SpritesU::drawBasic(
    int16_t x, int16_t y, uint8_t w, uint8_t h,
    uint24_t image, uint16_t frame, uint8_t mode)
{
    if(x >= 128) return;
    if(y >= 64)  return;
    if(x + w <= 0) return;
    if(y + h <= 0) return;
    
    uint8_t oldh = h;    
    
#ifdef ARDUINO_ARCH_AVR

    /*
              A1 A0
              B1 B0
           ========
              A0*B0
           A0*B1
           A1*B0
        A1*B1
        ===========
           C2 C1 C0
    */
    
    uint16_t tmp;
    asm volatile(R"ASM(
            cp   %A[frame], __zero_reg__
            cpc  %B[frame], __zero_reg__
            breq 1f
            
            ; add frame offset to image
            lsr  %[h]
            lsr  %[h]
            lsr  %[h]
            sbrc %[mode], 0
            lsl  %A[h]
            mul  %A[h], %[w]
            movw %A[tmp], r0
            
            mul  %A[tmp], %B[frame]
            add  %B[image], r0
            adc  %C[image], r1
            mul  %B[tmp], %A[frame]
            add  %B[image], r0
            adc  %C[image], r1
            mul  %B[tmp], %B[frame]
            add  %C[image], r0
            mul  %A[tmp], %A[frame]
            add  %A[image], r0
            adc  %B[image], r1
            clr  __zero_reg__
            adc  %C[image], __zero_reg__
            
        1:
        )ASM"
        :
        [h]     "+&r" (h),
        [image] "+&r" (image),
        [tmp]   "=&r" (tmp)
        :
        [frame] "r"   (frame),
        [mode]  "r"   (mode),
        [w]     "r"   (w)
        );
#else
    if(frame != 0)
    {
        h >>= 3;
        if(mode & 1) h <<= 1;
        uint16_t tmp = h * w;
        image += uint24_t(tmp) * frame;
    }
#endif

    drawBasicNoChecks((uint16_t(oldh) << 8) | w, image, mode, x, y);
}

void SpritesU::drawBasicNoChecks(
    uint16_t w_and_h,
    uint24_t image, uint8_t mode,
    int16_t x, int16_t y)
{
    uint8_t* buf;
    uint8_t pages;
    uint8_t count;
    uint8_t buf_data;
    uint16_t image_data;
    uint8_t cols;
    uint8_t buf_adv;
    uint16_t image_adv;
    uint16_t shift_mask;
    uint8_t shift_coef;
    bool bottom;
    int8_t page_start;
    uint8_t w;
    uint16_t mask_data;

    {
    uint8_t h;
    uint8_t col_start;
    
    w = uint8_t(w_and_h);
    h = uint8_t(w_and_h >> 8);
    buf = Arduboy2Base::sBuffer;
    pages = h;
    
#ifdef ARDUINO_ARCH_AVR
    asm volatile(R"ASM(
            mov  %[col_start], %A[x]
            clr  %[bottom]
            mov  %[cols], %[w]
    
            lsr  %[pages]
            lsr  %[pages]
            lsr  %[pages]
            
            ; precompute vertical shift coef and mask
            ldi  %[shift_coef], 1
            sbrc %A[y], 1
            ldi  %[shift_coef], 4
            sbrc %A[y], 0
            lsl  %[shift_coef]
            sbrc %A[y], 2
            swap %[shift_coef]
            clr  %A[shift_mask]
            com  %A[shift_mask]
            mov  %B[shift_mask], %A[shift_mask]
            sbrc %[mode], 2
            rjmp 1f
            ldi  %[buf_adv], 0xff
            mul  %[buf_adv], %[shift_coef]
            movw %A[shift_mask], r0
            com  %A[shift_mask]
            com  %B[shift_mask]
        1:
            
            asr  %B[y]
            ror  %A[y]
            asr  %B[y]
            ror  %A[y]
            asr  %B[y]
            ror  %A[y]
            
            ; clip against top edge
            mov  %[page_start], %A[y]
            cpi  %[page_start], 0xff
            brge 2f
            com  %[page_start]
            sub  %[pages], %[page_start]
            sbrc %[mode], 0
            lsl  %[page_start]
            mul  %[page_start], %[w]
            add  %A[image], r0
            adc  %B[image], r1
            adc  %C[image], %[bottom]
            ldi  %[page_start], 0xff
        2:
            ; clip against left edge
            sbrs %B[x], 7
            rjmp 4f
            add %[cols], %A[x]
            sbrs %[mode], 0
            rjmp 3f
            lsl  %A[x]
            rol  %B[x]
        3:
            sub  %A[image], %A[x]
            sbc  %B[image], %B[x]
            sbc  %C[image], %[bottom]
            sbrc %B[x], 7
            inc  %C[image]
            clr  %[col_start]
        4:
            ; compute buffer start address
            ldi  %[buf_adv], 128
            mulsu %[page_start], %[buf_adv]
            add  r0, %[col_start]
            add  %A[buf], r0
            adc  %B[buf], r1
            
            ; clip against right edge
            sub  %[buf_adv], %[col_start]
            cp   %[cols], %[buf_adv]
            brlo 5f
            mov  %[cols], %[buf_adv]
        5:
            ; clip against bottom edge
            ldi  %[buf_adv], 7
            sub  %[buf_adv], %[page_start]
            cp   %[buf_adv], %[pages]
            brge 6f
            mov  %[pages], %[buf_adv]
            inc  %[bottom]
        6:
            ldi  %[buf_adv], 128
            sub  %[buf_adv], %[cols]
            mov  %A[image_adv], %[w]
            clr  %B[image_adv]
            sbrc %[mode], 1
            rjmp 7f
            sub  %A[image_adv], %[cols]
            sbc  %B[image_adv], %B[image_adv]
        7:
            sbrs %[mode], 0
            rjmp 8f
            lsl  %A[image_adv]
            rol  %B[image_adv]
        8:
            clr __zero_reg__
        )ASM"
        :
        [pages]      "+&r" (pages),
        [shift_coef] "=&d" (shift_coef),
        [shift_mask] "=&r" (shift_mask),
        [page_start] "=&a" (page_start),
        [cols]       "=&r" (cols),
        [col_start]  "=&r" (col_start),
        [bottom]     "=&r" (bottom),
        [buf]        "+&r" (buf),
        [buf_adv]    "=&a" (buf_adv),
        [image_adv]  "=&r" (image_adv),
        [x]          "+&r" (x),
        [y]          "+&r" (y),
        [image]      "+&r" (image)
        :
        [mode]       "r"   (mode),
        [w]          "r"   (w)
        );
    
#else
    col_start = uint8_t(x);
    bottom = false;
    cols = w;

    pages >>= 3;

    // precompute vertical shift coef and mask
    shift_coef = SpritesU_bitShiftLeftUInt8(y);
    if(mode & 4)
        shift_mask = 0xffff;
    else
        shift_mask = ~(0xff * shift_coef);

    // y /= 8 (round to -inf)
    y >>= 3;
    
    // clip against top edge
    page_start = int8_t(y);
    if(page_start < -1)
    {
        page_start = ~page_start;
        pages -= page_start;
        if(mode & 1) page_start <<= 1;
        image += (uint8_t)page_start * w;
        page_start = -1;
    }

    // clip against left edge
    if(x < 0)
    {
        cols += x;
        if(mode & 1) x *= 2;
        image -= x;
        col_start = 0;
    }

    // compute buffer start address
    buf_adv = 128;
    buf += page_start * buf_adv + col_start;

    // clip against right edge
    buf_adv -= col_start;
    if(cols >= buf_adv)
        cols = buf_adv;

    // clip against bottom edge
    buf_adv = 7;
    buf_adv -= page_start;
    if(buf_adv < pages)
    {
        pages = buf_adv;
        bottom = true;
    }
    buf_adv = 128;
    buf_adv -= cols;
    image_adv = w;
    if(!(mode & 2))
        image_adv -= cols;
    if(mode & 1)
        image_adv <<= 1;

#endif

    }

#ifdef SPRITESU_OVERWRITE
    if(mode == MODE_OVERWRITE)
    {
        uint8_t const* image_ptr = (uint8_t const*)image;
#ifdef ARDUINO_ARCH_AVR
        asm volatile(R"ASM(

                cp  %[page_start], __zero_reg__
                brge L%=_middle

                ; advance buf to next page
                subi %A[buf], lo8(-128)
                sbci %B[buf], hi8(-128)
                mov %[count], %[cols]

            L%=_top_loop:

                ; write one page from image to buf+128
                lpm %A[image_data], %a[image]+
                mul %A[image_data], %[shift_coef]
                ld %[buf_data], %a[buf]
                and %[buf_data], %B[shift_mask]
                or %[buf_data], r1
                st %a[buf]+, %[buf_data]
                dec %[count]
                brne L%=_top_loop

                ; decrement pages, reset buf back, advance image
                clr __zero_reg__
                dec %[pages]
                sub %A[buf], %[cols]
                sbc %B[buf], __zero_reg__
                add %A[image], %A[image_adv]
                adc %B[image], %B[image_adv]

            L%=_middle:

                tst %[pages]
                breq L%=_bottom

                ; need Y pointer for middle pages
                push r28
                push r29
                movw r28, %[buf]
                subi r28, lo8(-128)
                sbci r29, hi8(-128)

            L%=_middle_loop_outer:

                mov %[count], %[cols]

            L%=_middle_loop_inner:

                ; write one page from image to buf/buf+128
                lpm %A[image_data], %a[image]+
                mul %A[image_data], %[shift_coef]
                ld %[buf_data], %a[buf]
                and %[buf_data], %A[shift_mask]
                or %[buf_data], r0
                st %a[buf]+, %[buf_data]
                ld %[buf_data], Y
                and %[buf_data], %B[shift_mask]
                or %[buf_data], r1
                st Y+, %[buf_data]
                dec %[count]
                brne L%=_middle_loop_inner

                ; advance buf, buf+128, and image to the next page
                clr __zero_reg__
                add %A[buf], %[buf_adv]
                adc %B[buf], __zero_reg__
                add r28, %[buf_adv]
                adc r29, __zero_reg__
                add %A[image], %A[image_adv]
                adc %B[image], %B[image_adv]
                dec %[pages]
                brne L%=_middle_loop_outer

                ; done with Y pointer
                pop r29
                pop r28

            L%=_bottom:

                tst %[bottom]
                breq L%=_finish

            L%=_bottom_loop:

                ; write one page from image to buf
                lpm %A[image_data], %a[image]+
                mul %A[image_data], %[shift_coef]
                ld %[buf_data], %a[buf]
                and %[buf_data], %A[shift_mask]
                or %[buf_data], r0
                st %a[buf]+, %[buf_data]
                dec %[cols]
                brne L%=_bottom_loop

            L%=_finish:

                clr __zero_reg__

            )ASM"
            :
            [buf]        "+&x" (buf),
            [image]      "+&z" (image_ptr),
            [pages]      "+&r" (pages),
            [count]      "=&r" (count),
            [buf_data]   "=&r" (buf_data),
            [cols]       "+&r" (cols),
            [image_data] "=&r" (image_data)
            :
            [buf_adv]    "r"   (buf_adv),
            [image_adv]  "r"   (image_adv),
            [shift_mask] "r"   (shift_mask),
            [shift_coef] "r"   (shift_coef),
            [bottom]     "r"   (bottom),
            [page_start] "r"   (page_start)
            :
            "r28", "r29", "memory"
            );
#else
        if(page_start < 0)
        {
            buf += 128;
            count = cols;
            do
            {
                image_data = pgm_read_byte(image_ptr++);
                uint16_t t = (uint8_t)image_data * shift_coef;
                buf_data = *buf;
                buf_data &= uint8_t(shift_mask >> 8);
                buf_data |= uint8_t(t >> 8);
                *buf++ = buf_data;
            } while(--count != 0);
            --pages;
            buf -= cols;
            image_ptr += image_adv;
        }
        if(pages != 0)
        {
            uint8_t* bufn = buf + 128;
            do
            {
                count = cols;
                do
                {
                    image_data = pgm_read_byte(image_ptr++);
                    uint16_t t = (uint8_t)image_data * shift_coef;
                    buf_data = *buf;
                    buf_data &= uint8_t(shift_mask >> 0);
                    buf_data |= uint8_t(t >> 0);
                    *buf++ = buf_data;
                    buf_data = *bufn;
                    buf_data &= uint8_t(shift_mask >> 8);
                    buf_data |= uint8_t(t >> 8);
                    *bufn++ = buf_data;
                } while(--count != 0);
                buf += buf_adv;
                bufn += buf_adv;
                image_ptr += image_adv;
            } while(--pages != 0);
        }
        if(bottom)
        {
            do
            {
                image_data = pgm_read_byte(image_ptr++);
                uint16_t t = (uint8_t)image_data * shift_coef;
                buf_data = *buf;
                buf_data &= uint8_t(shift_mask >> 0);
                buf_data |= uint8_t(t >> 0);
                *buf++ = buf_data;
            }
            while(--cols != 0);
        }
#endif
    }
    else
#endif
#ifdef SPRITESU_PLUSMASK
    if(mode == MODE_PLUSMASK)
    {
        uint8_t const* image_ptr = (uint8_t const*)image;
#ifdef ARDUINO_ARCH_AVR
        asm volatile(R"ASM(

                cp  %[page_start], __zero_reg__
                brge L%=_middle

                ; advance buf to next page
                subi %A[buf], lo8(-128)
                sbci %B[buf], hi8(-128)
                mov %[count], %[cols]

            L%=_top_loop:

                ; write one page from image to buf+128
                lpm %A[image_data], %a[image]+
                lpm %A[mask_data], %a[image]+

                mul %A[image_data], %[shift_coef]
                movw %[image_data], r0
                mul %A[mask_data], %[shift_coef]
                movw %[mask_data], r0

                ld %[buf_data], %a[buf]
                com %B[mask_data]
                and %[buf_data], %B[mask_data]
                or %[buf_data], %B[image_data]
                st %a[buf]+, %[buf_data]
                dec %[count]
                brne L%=_top_loop

                ; decrement pages, reset buf back, advance image and mask
                clr __zero_reg__
                dec %[pages]
                sub %A[buf], %[cols]
                sbc %B[buf], __zero_reg__
                add %A[image], %[image_adv]
                adc %B[image], __zero_reg__

            L%=_middle:

                tst %[pages]
                breq L%=_bottom

                ; need Y pointer for middle pages
                push r28
                push r29
                movw r28, %[buf]
                subi r28, lo8(-128)
                sbci r29, hi8(-128)

            L%=_middle_loop_outer:

                mov %[count], %[cols]

            L%=_middle_loop_inner:
                ; write one page from image to buf/buf+128
                lpm %A[image_data], %a[image]+
                lpm %A[mask_data], %a[image]+

                mul %A[image_data], %[shift_coef]
                movw %[image_data], r0
                mul %A[mask_data], %[shift_coef]
                movw %[mask_data], r0

                ld %[buf_data], %a[buf]
                com %A[mask_data]
                and %[buf_data], %A[mask_data]
                or %[buf_data], %A[image_data]
                st %a[buf]+, %[buf_data]
                ld %[buf_data], Y
                com %B[mask_data]
                and %[buf_data], %B[mask_data]
                or %[buf_data], %B[image_data]
                st Y+, %[buf_data]
                dec %[count]
                brne L%=_middle_loop_inner

                ; advance buf, buf+128, and image to the next page
                clr __zero_reg__
                add %A[buf], %[buf_adv]
                adc %B[buf], __zero_reg__
                add r28, %[buf_adv]
                adc r29, __zero_reg__
                add %A[image], %[image_adv]
                adc %B[image], __zero_reg__
                dec %[pages]
                brne L%=_middle_loop_outer

                ; done with Y pointer
                pop r29
                pop r28

            L%=_bottom:

                tst %[bottom]
                breq L%=_finish

            L%=_bottom_loop:

                ; write one page from image to buf
                lpm %A[image_data], %a[image]+
                lpm %A[mask_data], %a[image]+
                mul %A[image_data], %[shift_coef]
                movw %[image_data], r0
                mul %A[mask_data], %[shift_coef]
                movw %[mask_data], r0

                ld %[buf_data], %a[buf]
                com %A[mask_data]
                and %[buf_data], %A[mask_data]
                or %[buf_data], %A[image_data]
                st %a[buf]+, %[buf_data]
                dec %[cols]
                brne L%=_bottom_loop

            L%=_finish:

                clr __zero_reg__

            )ASM"
            :
            [buf]        "+&x" (buf),
            [image]      "+&z" (image_ptr),
            [pages]      "+&r" (pages),
            [count]      "=&r" (count),
            [buf_data]   "=&r" (buf_data),
            [image_data] "=&r" (image_data),
            [cols]       "+&r" (cols),
            [mask_data]  "=&r" (mask_data)
            :
            [buf_adv]    "r"   (buf_adv),
            [image_adv]  "r"   (image_adv),
            [shift_coef] "r"   (shift_coef),
            [bottom]     "r"   (bottom),
            [page_start] "r"   (page_start)
            :
            "r28", "r29", "memory"
            );
#else
        if(page_start < 0)
        {
            buf += 128;
            count = cols;
            do
            {
                image_data = pgm_read_byte(image_ptr++);
                mask_data = pgm_read_byte(image_ptr++);
                image_data = (uint8_t)image_data * shift_coef;
                mask_data = (uint8_t)mask_data * shift_coef;
                buf_data = *buf;
                buf_data &= ~uint8_t(mask_data >> 8);
                buf_data |= uint8_t(image_data >> 8);
                *buf++ = buf_data;
            } while(--count != 0);
            --pages;
            buf -= cols;
            image_ptr += image_adv;
        }
        if(pages != 0)
        {
            uint8_t* bufn = buf + 128;
            do
            {
                count = cols;
                do
                {
                    image_data = pgm_read_byte(image_ptr++);
                    mask_data = pgm_read_byte(image_ptr++);
                    image_data = (uint8_t)image_data * shift_coef;
                    mask_data = (uint8_t)mask_data * shift_coef;
                    buf_data = *buf;
                    buf_data &= ~uint8_t(mask_data >> 0);
                    buf_data |= uint8_t(image_data >> 0);
                    *buf++ = buf_data;
                    buf_data = *bufn;
                    buf_data &= ~uint8_t(mask_data >> 8);
                    buf_data |= uint8_t(image_data >> 8);
                    *bufn++ = buf_data;
                } while(--count != 0);
                buf += buf_adv;
                bufn += buf_adv;
                image_ptr += image_adv;
            } while(--pages != 0);
        }
        if(bottom)
        {
            do
            {
                image_data = pgm_read_byte(image_ptr++);
                mask_data = pgm_read_byte(image_ptr++);
                image_data = (uint8_t)image_data * shift_coef;
                mask_data = (uint8_t)mask_data * shift_coef;
                buf_data = *buf;
                buf_data &= ~uint8_t(mask_data >> 0);
                buf_data |= uint8_t(image_data >> 0);
                *buf++ = buf_data;
            }
            while(--cols != 0);
        }
#endif
    }
    else
#endif
#ifdef SPRITESU_FX
    {
        uint8_t sfc_read = SFC_READ;
        uint8_t* bufn;
        uint8_t reseek;
#ifdef ARDUINO_ARCH_AVR
        asm volatile(R"ASM(

                lds r0, %[page]+0            ; 2
                add %B[image], r0            ; 1
                lds r0, %[page]+1            ; 2
                adc %C[image], r0            ; 1
                rjmp L%=_begin

            L%=_seek:

                ; seek subroutine
                cbi %[fxport], %[fxbit]
                out %[spdr], %[sfc_read]
                add %A[image], %A[image_adv] ;  1
                adc %B[image], %B[image_adv] ;  1
                adc %C[image], __zero_reg__  ;  1
                clr %[reseek]                ;  1
                cp  %[w], %[cols]            ;  1
                breq .+2                     ;  1
                inc %[reseek]                ;  1
                rcall L%=_delay_10           ; 10
                out %[spdr], %C[image]
                rcall L%=_delay_17
                out %[spdr], %B[image]
                rcall L%=_delay_17
                out %[spdr], %A[image]
                rcall L%=_delay_17
                out %[spdr], __zero_reg__
                rcall L%=_delay_13
                ret
                
            L%=_delay_17:
                lpm
            L%=_delay_14:
                nop
            L%=_delay_13:
                lpm
            L%=_delay_10:
                lpm
            L%=_delay_7:
                ret

            L%=_begin:

                ; initial seek
                sub %A[image], %A[image_adv]
                sbc %B[image], %B[image_adv]
                sbc %C[image], __zero_reg__
                rcall L%=_seek
                cp %[page_start], __zero_reg__
                brlt L%=_top
                tst %[pages]
                brne L%=_middle_skip_reseek
                rjmp L%=_bottom_dispatch

            L%=_top:

                ; init buf
                subi %A[buf], lo8(-128)
                sbci %B[buf], hi8(-128)
                mov %[count], %[cols]

                ; loop dispatch
                sbrc %[mode], 0
                rjmp L%=_top_loop_masked

            L%=_top_loop:

                in %A[image_data], %[spdr]
                out %[spdr], __zero_reg__
                mul %A[image_data], %[shift_coef]
                ld %[buf_data], %a[buf]
                and %[buf_data], %B[shift_mask]
                or %[buf_data], r1
                st %a[buf]+, %[buf_data]
                lpm
                rjmp .+0
                dec %[count]
                brne L%=_top_loop
                rjmp L%=_top_loop_done

            L%=_top_loop_masked:

                in %A[image_data], %[spdr]
                out %[spdr], __zero_reg__
                mul %A[image_data], %[shift_coef]
                movw %A[image_data], r0
                rcall L%=_delay_13
                in %A[shift_mask], %[spdr]
                out %[spdr], __zero_reg__
                mul %A[shift_mask], %[shift_coef]
                movw %A[shift_mask], r0
                ld %[buf_data], %a[buf]
                com %B[shift_mask]
                and %[buf_data], %B[shift_mask]
                or %[buf_data], %B[image_data]
                st %a[buf]+, %[buf_data]
                lpm
                dec %[count]
                brne L%=_top_loop_masked

            L%=_top_loop_done:

                ; decrement pages, reset buf back
                clr __zero_reg__
                sub %A[buf], %[cols]
                sbc %B[buf], __zero_reg__
                dec %[pages]
                brne L%=_middle
                rjmp L%=_finish

            L%=_middle:

                ; only seek again if necessary
                tst %[reseek]
                breq L%=_middle_skip_reseek
                in r0, %[spsr]
                sbi %[fxport], %[fxbit]
                rcall L%=_seek

            L%=_middle_skip_reseek:

                movw %[bufn], %[buf]
                subi %A[bufn], lo8(-128)
                sbci %B[bufn], hi8(-128)

            L%=_middle_loop_outer:

                mov %[count], %[cols]

                ; loop dispatch
                sbrc %[mode], 0
                rjmp L%=_middle_loop_inner_masked

            L%=_middle_loop_inner:

                ; write one page from image to buf/buf+128
                in %A[image_data], %[spdr]
                out %[spdr], __zero_reg__
                mul %A[image_data], %[shift_coef]
                ld %[buf_data], %a[buf]
                and %[buf_data], %A[shift_mask]
                or %[buf_data], r0
                st %a[buf]+, %[buf_data]
                ld %[buf_data], %a[bufn]
                and %[buf_data], %B[shift_mask]
                or %[buf_data], r1
                st %a[bufn]+, %[buf_data]
                dec %[count]
                brne L%=_middle_loop_inner
                rjmp L%=_middle_loop_outer_next

            L%=_middle_loop_inner_masked:

                ; write one page from image to buf/buf+128
                in %A[image_data], %[spdr]
                out %[spdr], __zero_reg__
                mul %A[image_data], %[shift_coef]
                movw %A[image_data], r0
                ld %[buf_data], %a[buf]
                ld %B[shift_mask], %a[bufn]
                rcall L%=_delay_7
                rjmp .+0
                in %A[shift_mask], %[spdr]
                out %[spdr], __zero_reg__
                mul %A[shift_mask], %[shift_coef]
                com r0
                and %[buf_data], r0
                or %[buf_data], %A[image_data]
                st %a[buf]+, %[buf_data]
                com r1
                and %B[shift_mask], r1
                or %B[shift_mask], %B[image_data]
                st %a[bufn]+, %B[shift_mask]
                nop
                dec %[count]
                brne L%=_middle_loop_inner_masked

            L%=_middle_loop_outer_next:

                ; advance buf to the next page
                clr __zero_reg__
                add %A[buf], %[buf_adv]
                adc %B[buf], __zero_reg__
                dec %[pages]
                brne L%=_middle

            L%=_bottom:

                tst %[bottom]
                breq L%=_finish

                ; seek if needed
                tst %[reseek]
                breq L%=_bottom_dispatch
                in r0, %[spsr]
                sbi %[fxport], %[fxbit]
                rcall L%=_seek
                lpm

            L%=_bottom_dispatch:

                ; loop dispatch
                sbrc %[mode], 0
                rjmp L%=_bottom_loop_masked

            L%=_bottom_loop:

                ; write one page from image to buf
                in %A[image_data], %[spdr]
                out %[spdr], __zero_reg__
                mul %A[image_data], %[shift_coef]
                ld %[buf_data], %a[buf]
                and %[buf_data], %A[shift_mask]
                or %[buf_data], r0
                st %a[buf]+, %[buf_data]
                lpm
                rjmp .+0
                dec %[cols]
                brne L%=_bottom_loop
                rjmp L%=_finish

            L%=_bottom_loop_masked:

                ; write one page from image to buf
                in %A[image_data], %[spdr]
                out %[spdr], __zero_reg__
                mul %A[image_data], %[shift_coef]
                movw %A[image_data], r0
                rcall L%=_delay_13
                in %[pages], %[spdr]
                out %[spdr], __zero_reg__
                mul %[pages], %[shift_coef]
                mov %[pages], r0
                ld %[buf_data], %a[buf]
                com %[pages]
                and %[buf_data], %[pages]
                or %[buf_data], %A[image_data]
                st %a[buf]+, %[buf_data]
                lpm
                dec %[cols]
                brne L%=_bottom_loop_masked
                lpm

            L%=_finish:

                clr __zero_reg__
                sbi %[fxport], %[fxbit]
                in r0, %[spsr]

            )ASM"
            :
            [buf]        "+&x" (buf),
            [bufn]       "=&z" (bufn),
            [image]      "+&r" (image),
            [pages]      "+&r" (pages),
            [count]      "=&r" (count),
            [buf_data]   "=&r" (buf_data),
            [image_data] "=&r" (image_data),
            [cols]       "+&r" (cols),
            [reseek]     "=&r" (reseek)
            :
            [w]          "r"   (w),
            [buf_adv]    "r"   (buf_adv),
            [image_adv]  "r"   (image_adv),
            [shift_coef] "r"   (shift_coef),
            [shift_mask] "r"   (shift_mask),
            [bottom]     "r"   (bottom),
            [page_start] "r"   (page_start),
            [mode]       "r"   (mode),
            [sfc_read]   "r"   (sfc_read),
            [fxport]     "I"   (_SFR_IO_ADDR(FX_PORT)),
            [fxbit]      "I"   (FX_BIT),
            [spdr]       "I"   (_SFR_IO_ADDR(SPDR)),
            [spsr]       "I"   (_SFR_IO_ADDR(SPSR)),
            [spif]       "I"   (SPIF),
            [page]       "i"   (&FX::programDataPage)
            :
            "memory"
            );
#else
        reseek = false;
        FX::seekData(image);
        
        if(page_start < 0)
        {
            // top
            buf += 128;
            count = cols;
            if(!(mode & 1))
            {
                do
                {
                    image_data = FX::readPendingUInt8();
                    image_data = (uint8_t)image_data * shift_coef;
                    buf_data = *buf;
                    buf_data &= uint8_t(shift_mask >> 8);
                    buf_data |= uint8_t(image_data >> 8);
                    *buf++ = buf_data;
                } while(--count != 0);
            }
            else
            {
                do
                {
                    image_data = FX::readPendingUInt8();
                    image_data = (uint8_t)image_data * shift_coef;
                    shift_mask = FX::readPendingUInt8();
                    shift_mask = (uint8_t)shift_mask * shift_coef;
                    buf_data = *buf;
                    buf_data &= ~uint8_t(shift_mask >> 8);
                    buf_data |= uint8_t(image_data >> 8);
                    *buf++ = buf_data;
                } while(--count != 0);
            }
            --pages;
            buf -= cols;
            reseek = (w != cols);
        }
        
        if(pages != 0)
        {
        
            do
            {
                if(reseek)
                {
                    (void)FX::readEnd();
                    image += image_adv;
                    FX::seekData(image);
                }
                reseek = (w != cols);
                
                bufn = buf + 128;
                count = cols;
                if(!(mode & 1))
                {
                    do
                    {
                        image_data = FX::readPendingUInt8();
                        image_data = (uint8_t)image_data * shift_coef;
                        buf_data = *buf;
                        buf_data &= uint8_t(shift_mask >> 0);
                        buf_data |= uint8_t(image_data >> 0);
                        *buf++ = buf_data;
                        buf_data = *bufn;
                        buf_data &= uint8_t(shift_mask >> 8);
                        buf_data |= uint8_t(image_data >> 8);
                        *bufn++ = buf_data;
                    } while(--count != 0);
                }
                else
                {
                    do
                    {
                        image_data = FX::readPendingUInt8();
                        image_data = (uint8_t)image_data * shift_coef;
                        shift_mask = FX::readPendingUInt8();
                        shift_mask = (uint8_t)shift_mask * shift_coef;
                        buf_data = *buf;
                        buf_data &= ~uint8_t(shift_mask >> 0);
                        buf_data |= uint8_t(image_data >> 0);
                        *buf++ = buf_data;
                        buf_data = *bufn;
                        buf_data &= ~uint8_t(shift_mask >> 8);
                        buf_data |= uint8_t(image_data >> 8);
                        *bufn++ = buf_data;
                    } while(--count != 0);
                }
                buf += buf_adv;
            } while(--pages != 0);
        }
        
        if(bottom)
        {
            if(reseek)
            {
                (void)FX::readEnd();
                image += image_adv;
                FX::seekData(image);
            }
            
            if(!(mode & 1))
            {
                do
                {
                    image_data = FX::readPendingUInt8();
                    image_data = (uint8_t)image_data * shift_coef;
                    buf_data = *buf;
                    buf_data &= uint8_t(shift_mask >> 0);
                    buf_data |= uint8_t(image_data >> 0);
                    *buf++ = buf_data;
                } while(--cols != 0);
            }
            else
            {
                do
                {
                    image_data = FX::readPendingUInt8();
                    image_data = (uint8_t)image_data * shift_coef;
                    shift_mask = FX::readPendingUInt8();
                    shift_mask = (uint8_t)shift_mask * shift_coef;
                    buf_data = *buf;
                    buf_data &= ~uint8_t(shift_mask >> 0);
                    buf_data |= uint8_t(image_data >> 0);
                    *buf++ = buf_data;
                } while(--cols != 0);
            }
        }
    
        (void)FX::readEnd();
#endif
    }
#endif
    {} // empty final else block, if needed
}

#ifdef SPRITESU_OVERWRITE
void SpritesU::drawOverwrite(
    int16_t x, int16_t y, uint8_t const* image, uint16_t frame)
{
    uint8_t w, h;
#ifdef ARDUINO_ARCH_AVR
    asm volatile(
        "lpm %[w], Z+\n"
        "lpm %[h], Z+\n"
        : [w] "=r" (w), [h] "=r" (h), [image] "+z" (image));
#else
    w = pgm_read_byte(image++);
    h = pgm_read_byte(image++);
#endif
    drawBasic(x, y, w, h, (uint24_t)image, frame, MODE_OVERWRITE);
}
void SpritesU::drawOverwrite(
    int16_t x, int16_t y, uint8_t w, uint8_t h, uint8_t const* image)
{
    drawBasic(x, y, w, h, (uint24_t)image, 0, MODE_OVERWRITE);
}
#endif

#ifdef SPRITESU_PLUSMASK
void SpritesU::drawPlusMask(
    int16_t x, int16_t y, uint8_t const* image, uint16_t frame)
{
    uint8_t w, h;
#ifdef ARDUINO_ARCH_AVR
    asm volatile(
        "lpm %[w], Z+\n"
        "lpm %[h], Z+\n"
        : [w] "=r" (w), [h] "=r" (h), [image] "+z" (image));
#else
    w = pgm_read_byte(image++);
    h = pgm_read_byte(image++);
#endif
    drawBasic(x, y, w, h, (uint24_t)image, frame, MODE_PLUSMASK);
}
void SpritesU::drawPlusMask(
    int16_t x, int16_t y, uint8_t w, uint8_t h, uint8_t const* image)
{
    drawBasic(x, y, w, h, (uint24_t)image, 0, MODE_PLUSMASK);
}
#endif

#if defined(SPRITESU_OVERWRITE) || defined(SPRITESU_PLUSMASK)
void SpritesU::drawSelfMask(
    int16_t x, int16_t y, uint8_t const* image, uint16_t frame)
{
    uint8_t w, h;
#ifdef ARDUINO_ARCH_AVR
    asm volatile(
        "lpm %[w], Z+\n"
        "lpm %[h], Z+\n"
        : [w] "=r" (w), [h] "=r" (h), [image] "+z" (image));
#else
    w = pgm_read_byte(image++);
    h = pgm_read_byte(image++);
#endif
    drawBasic(x, y, w, h, (uint24_t)image, frame, MODE_SELFMASK);
}
void SpritesU::drawSelfMask(
    int16_t x, int16_t y, uint8_t w, uint8_t h, uint8_t const* image)
{
    drawBasic(x, y, w, h, (uint24_t)image, 0, MODE_SELFMASK);
}
#endif

#ifdef SPRITESU_FX
void SpritesU::drawOverwriteFX(
    int16_t x, int16_t y, uint24_t image, uint16_t frame)
{
    FX::seekData(image);
    uint8_t w = FX::readPendingUInt8();
    uint8_t h = FX::readEnd();
    drawBasic(x, y, w, h, image + 2, frame, MODE_OVERWRITEFX);
}
void SpritesU::drawOverwriteFX(
    int16_t x, int16_t y, uint8_t w, uint8_t h, uint24_t image, uint16_t frame)
{
    drawBasic(x, y, w, h, image + 2, frame, MODE_OVERWRITEFX);
}
void SpritesU::drawPlusMaskFX(
    int16_t x, int16_t y, uint24_t image, uint16_t frame)
{
    FX::seekData(image);
    uint8_t w = FX::readPendingUInt8();
    uint8_t h = FX::readEnd();
    drawBasic(x, y, w, h, image + 2, frame, MODE_PLUSMASKFX);
}
void SpritesU::drawPlusMaskFX(
    int16_t x, int16_t y, uint8_t w, uint8_t h, uint24_t image, uint16_t frame)
{
    drawBasic(x, y, w, h, image + 2, frame, MODE_PLUSMASKFX);
}
void SpritesU::drawSelfMaskFX(
    int16_t x, int16_t y, uint24_t image, uint16_t frame)
{
    FX::seekData(image);
    uint8_t w = FX::readPendingUInt8();
    uint8_t h = FX::readEnd();
    drawBasic(x, y, w, h, image + 2, frame, MODE_SELFMASKFX);
}
void SpritesU::drawSelfMaskFX(
    int16_t x, int16_t y, uint8_t w, uint8_t h, uint24_t image, uint16_t frame)
{
    drawBasic(x, y, w, h, image + 2, frame, MODE_SELFMASKFX);
}
#endif

#ifdef SPRITESU_RECT
void SpritesU::fillRect(int16_t x, int16_t y, uint8_t w, uint8_t h, uint8_t color)
{
    if(x >= 128) return;
    if(x + w <= 0) return;
    if(y + h <= 0) return;
    fillRect_i8((int8_t)x, (int8_t)y, w, h, color);
}

// from Mr. Blinky's ArduboyFX library
static __attribute__((always_inline)) uint8_t SpritesU_bitShiftLeftMaskUInt8(uint8_t bit)
{
#ifdef ARDUINO_ARCH_AVR
    uint8_t result;
    asm volatile(
        "ldi    %[result], 1    \n" // 0 = 000 => 1111 1111 = -1
        "sbrc   %[bit], 1       \n" // 1 = 001 => 1111 1110 = -2
        "ldi    %[result], 4    \n" // 2 = 010 => 1111 1100 = -4
        "sbrc   %[bit], 0       \n" // 3 = 011 => 1111 1000 = -8
        "lsl    %[result]       \n"
        "sbrc   %[bit], 2       \n" // 4 = 100 => 1111 0000 = -16
        "swap   %[result]       \n" // 5 = 101 => 1110 0000 = -32
        "neg    %[result]       \n" // 6 = 110 => 1100 0000 = -64
        :[result] "=&d" (result)    // 7 = 111 => 1000 0000 = -128
        :[bit]    "r"   (bit)
        :
    );
    return result;
#else
    return (0xFF << (bit & 7)) & 0xFF;
#endif
}

void SpritesU::fillRect_i8(int8_t x, int8_t y, uint8_t w, uint8_t h, uint8_t color)
{
    if(w == 0 || h == 0) return;
    if(y >= 64)  return;
    if(x + w <= 0) return;
    if(y + h <= 0) return;

    if(color & 1) color = 0xff;

    // clip coords
    uint8_t xc = x;
    uint8_t yc = y;

    // TODO: extreme clipping behavior

    // clip
    if(y < 0)
        h += y, yc = 0;
    if(x < 0)
        w += x, xc = 0;
    if(h >= uint8_t(64 - yc))
        h = 64 - yc;
    if(w >= uint8_t(128 - xc))
        w = 128 - xc;
    uint8_t y1 = yc + h;

    uint8_t c0 = SpritesU_bitShiftLeftMaskUInt8(yc); // 11100000
    uint8_t m1 = SpritesU_bitShiftLeftMaskUInt8(y1); // 11000000
    uint8_t m0 = ~c0; // 00011111
    uint8_t c1 = ~m1; // 00111111

    uint8_t r0 = yc;
    uint8_t r1 = y1 - 1;
#ifdef ARDUINO_ARCH_AVR
    asm volatile(
        "lsr %[r0]\n"
        "lsr %[r0]\n"
        "lsr %[r0]\n"
        "lsr %[r1]\n"
        "lsr %[r1]\n"
        "lsr %[r1]\n"
        : [r0] "+&r" (r0),
          [r1] "+&r" (r1));
#else
    r0 >>= 3;
    r1 >>= 3;
#endif

    uint8_t* buf = Arduboy2Base::sBuffer;
#ifdef ARDUINO_ARCH_AVR
    asm volatile(
        "mul %[r0], %[c128]\n"
        "add %A[buf], r0\n"
        "adc %B[buf], r1\n"
        "clr __zero_reg__\n"
        "add %A[buf], %[x]\n"
        "adc %B[buf], __zero_reg__\n"
        :
        [buf]  "+&e" (buf)
        :
        [r0]   "r"   (r0),
        [x]    "r"   (xc),
        [c128] "r"   ((uint8_t)128)
        );
#else
    buf += r0 * 128 + xc;
#endif

    uint8_t rows = r1 - r0; // middle rows + 1
    uint8_t f = 0;
    uint8_t bot = c1;
    if(m0  == 0) ++rows; // no top fragment
    if(bot == 0) ++rows; // no bottom fragment
    c0 &= color;
    c1 &= color;

    uint8_t col;
    uint8_t buf_adv = 128 - w;

#ifdef ARDUINO_ARCH_AVR
    asm volatile(R"ASM(
            tst  %[rows]
            brne L%=_top
            or   %[m1], %[m0]
            and  %[c1], %[c0]
            rjmp L%=_bottom_loop

        L%=_top:
            tst  %[m0]
            breq L%=_middle
            mov  %[col], %[w]

        L%=_top_loop:
            ld   __tmp_reg__, %a[buf]
            and  __tmp_reg__, %[m0]
            or   __tmp_reg__, %[c0]
            st   %a[buf]+, __tmp_reg__
            dec  %[col]
            brne L%=_top_loop
            add  %A[buf], %[buf_adv]
            adc  %B[buf], __zero_reg__

        L%=_middle:
            dec  %[rows]
            breq L%=_bottom

        L%=_middle_outer_loop:
            mov  %[col], %[w]
            sbrs %[col], 0
            rjmp L%=_middle_inner_loop
            inc  %[col]
            rjmp L%=_middle_inner_loop_odd
            
        L%=_middle_inner_loop:
            st   %a[buf]+, %[color]
        L%=_middle_inner_loop_odd:
            st   %a[buf]+, %[color]
            subi %[col], 2
            brne L%=_middle_inner_loop
            add  %A[buf], %[buf_adv]
            adc  %B[buf], __zero_reg__
            dec  %[rows]
            brne L%=_middle_outer_loop

        L%=_bottom:
            tst  %[bot]
            breq L%=_finish

        L%=_bottom_loop:
            ld   __tmp_reg__, %a[buf]
            and  __tmp_reg__, %[m1]
            or   __tmp_reg__, %[c1]
            st   %a[buf]+, __tmp_reg__
            dec  %[w]
            brne L%=_bottom_loop

        L%=_finish:
        )ASM"
        :
        [buf]     "+&e" (buf),
        [w]       "+&r" (w),
        [rows]    "+&r" (rows),
        [col]     "=&r" (col)
        :
        [buf_adv] "r"   (buf_adv),
        [color]   "r"   (color),
        [m0]      "r"   (m0),
        [m1]      "r"   (m1),
        [c0]      "r"   (c0),
        [c1]      "r"   (c1),
        [bot]     "r"   (bot)
        );
#else
    if(rows == 0)
    {
        m1 |= m0;
        c1 &= c0;
    }
    else
    {
        if(m0 != 0)
        {
            col = w;
            do
            {
                uint8_t t = *buf;
                t &= m0;
                t |= c0;
                *buf++ = t;
            } while(--col != 0);
            buf += buf_adv;
        }
        
        if(--rows != 0)
        {
            do
            {
                col = w;
                do
                {
                    *buf++ = color;
                } while(--col != 0);
                buf += buf_adv;
            } while(--rows != 0);
        }
    }
    
    if(bot)
    {
        do
        {
            uint8_t t = *buf;
            t &= m1;
            t |= c1;
            *buf++ = t;
        } while(--w != 0);
    }
    
#endif
}
#endif

#endif
