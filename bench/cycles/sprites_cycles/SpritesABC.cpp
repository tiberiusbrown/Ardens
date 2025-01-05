#include "SpritesABC.hpp"

[[ gnu::naked, gnu::noinline ]]
static void SpritesABC::drawFX(
    int16_t x, int16_t y,
    uint24_t image, uint8_t mode, uint16_t frame)
{
    asm volatile(R"ASM(
    
            push  r10
            push  r11
            push  r12
            push  r14
    
            cbi   %[fxport], %[fxbit]
            ldi   r30, 3
            out   %[spdr], r30
            push  r15
            push  r16
            movw  r10, r14
            mov   r12, r16
            movw  r14, r18
            mov   r16, r20
            ldi   r30, 2
            add   r14, r30
            adc   r15, r1
            adc   r16, r1
            lds   r0, %[page]+0
            add   r19, r0
            lds   r0, %[page]+1
            adc   r20, r0
            out   %[spdr], r20
            rcall L%=_delay_17
            out   %[spdr], r19
            rcall L%=_delay_17
            out   %[spdr], r18
            rcall L%=_delay_16
            out   %[spdr], r1
            rcall L%=_delay_16
            in    r20, %[spdr]
            out   %[spdr], r1
            rcall L%=_delay_16
            in    r18, %[spdr]
            sbi   %[fxport], %[fxbit]
            
            call  %x[drawf]
            pop   r16
            pop   r15
            pop   r14
            pop   r12
            pop   r11
            pop   r10
            ret
            
        L%=_delay_17:
            nop
        L%=_delay_16:
            lpm
        L%=_delay_13:
            rjmp .+0
        L%=_delay_11:
            lpm
        L%=_delay_8:
            nop
        L%=_delay_7:
            ret
            
        )ASM"
        :
        : [fxport]     "I"   (_SFR_IO_ADDR(FX_PORT))
        , [fxbit]      "I"   (FX_BIT)
        , [spdr]       "I"   (_SFR_IO_ADDR(SPDR))
        , [spsr]       "I"   (_SFR_IO_ADDR(SPSR))
        , [sreg]       "I"   (_SFR_IO_ADDR(SREG))
        , [page]       "i"   (&FX::programDataPage)
        , [drawf]      ""    (SpritesABC::drawSizedFX)
        );
}

[[ gnu::naked, gnu::noinline ]]
static void SpritesABC::drawSizedFX(
    int16_t x, int16_t y, uint8_t w, uint8_t h,
    uint24_t image, uint8_t mode, uint16_t frame)
{
    asm volatile(R"ASM(
    
        SpritesABC_drawSized:
        
            push r14
            push r15
            push r16
            
            ; w * (h >> 3) * (plusmask ? 2 : 1)
            mov  r0, r18
            lsr  r0
            lsr  r0
            lsr  r0
            sbrc r12, 0
            lsl  r0
            mul  r0, r20
            movw r30, r0
            
            ; image += frame * w * (h >> 3)
            mul  r30, r10
            add  r14, r0
            adc  r15, r1
            mul  r31, r10
            add  r15, r0
            adc  r16, r1
            mul  r30, r11
            add  r15, r0
            adc  r16, r1
            mul  r31, r11
            add  r16, r0
            clr  r1

        1:            
            
            call %x[drawf]
            
            pop  r16
            pop  r15
            pop  r14
    
        )ASM"
        :
        : [drawf] "" (SpritesABC::drawBasicFX)
        );
}

[[ gnu::naked, gnu::noinline ]]
void SpritesABC::drawBasicFX(
    int16_t x, int16_t y, uint8_t w, uint8_t h,
    uint24_t image, uint8_t mode)
{
    /*
    register bool     bottom     asm("r2");
    register uint8_t  col_start  asm("r3");
    register uint8_t  buf_adv    asm("r4");
    register uint8_t  shift_coef asm("r5");
    register uint16_t shift_mask asm("r6");
    register uint8_t  cols       asm("r8");
    register uint8_t  buf_data   asm("r9");
    register uint8_t  a_mode     asm("r12") = mode;
    register uint8_t  reseek     asm("r11");
    register uint24_t a_image    asm("r14") = image;
    register int8_t   page_start asm("r17");
    register uint8_t  a_h        asm("r18") = h;
    register uint8_t  pages      asm("r19");
    register uint8_t  a_w        asm("r20") = w;
    register uint8_t  count      asm("r21");
    register int16_t  a_y        asm("r22") = y;
    register int16_t  a_x        asm("r24") = x;
    register uint8_t* buf        asm("r26");
    register uint8_t* bufn       asm("r30");
    */

    asm volatile(R"ASM(
        
            cpi  r24, 128
            cpc  r25, __zero_reg__
            brge L%=_early_exit
        1:
            cpi  r22, 64
            cpc  r23, __zero_reg__
            brge L%=_early_exit
        1:
            movw r26, r24
            add  r26, r20
            adc  r27, __zero_reg__
            cp   __zero_reg__, r26
            cpc  __zero_reg__, r27
            brge L%=_early_exit
        1:
            movw r26, r22
            add  r26, r18
            adc  r27, __zero_reg__
            cp   __zero_reg__, r26
            cpc  __zero_reg__, r27
            brlt 1f
        
        L%=_early_exit:
            ret
        1:
    
            push r2
            push r3
            push r4
            push r5
            push r6
            push r7
            push r8
            push r9
            ; push r10 ; unmodified
            push r11
            ; push r12 ; unmodified
            ; push r13 ; unmodified
            push r14
            push r15
            push r16
            push r17
        
            mov  r3, r24
            clr  r2
            mov  r8, r20
    
            mov  r19, r18
            lsr  r19
            lsr  r19
            lsr  r19
    
            ; begin initial seek
            cbi  %[fxport], %[fxbit]
            ldi  r21, 3
            out  %[spdr], r21
            
            movw r30, r22
            asr  r31
            ror  r30
            asr  r31
            ror  r30
            asr  r31
            ror  r30
            
            ; clip against top edge
            mov  r17, r30
            cpi  r17, 0xff
            brge 1f
            com  r17
            sub  r19, r17
            sbrc r12, 0
            lsl  r17
            mul  r17, r20
            add  r14, r0
            adc  r15, r1
            adc  r16, r2
            ldi  r17, 0xff
        1:
        
            lds r0, %[page]+0
            add r15, r0
            lds r0, %[page]+1
            adc r16, r0
        
            ; clip against left edge
            sbrs r25, 7
            rjmp 2f
            add  r8, r24
            sbrs r12, 0
            rjmp 1f
            lsl  r24
            rol  r25
        1:  sub  r14, r24
            sbc  r15, r25
            sbc  r16, r2
            sbrc r25, 7
            inc  r16
            clr  r3
        2:
        
            ; continue initial seek
            out  %[spdr], r16
        
            ; compute buffer start address
            ldi  r26, lo8(%[sBuffer])
            ldi  r27, hi8(%[sBuffer])
            ldi  r21, 128
            mulsu r17, r21
            add  r0, r3
            add  r26, r0
            adc  r27, r1
            
            ; clip against right edge
            sub  r21, r3
            cp   r8, r21
            brlo 1f
            mov  r8, r21
        1:
        
            ; clip against bottom edge
            ldi  r30, 7
            sub  r30, r17
            
            cp   r30, r19
            brge 1f
            mov  r19, r30
            inc  r2
        1:
            
            ; continue initial seek
            out  %[spdr], r15
        
            ldi  r30, 128
            sub  r30, r8
            mov  r4, r30
            
            ; precompute vertical shift coef and mask
            ldi  r21, 1
            sbrc r22, 1
            ldi  r21, 4
            sbrc r22, 0
            lsl  r21
            sbrc r22, 2
            swap r21
            mov  r5, r21

            ldi  r30, 0xff
            mul  r30, r5
            movw r6, r0
            com  r6
            com  r7
            
            ; continue initial seek
            out  %[spdr], r14
        
            ; continue initial seek
            clr  __zero_reg__
            in   r22, %[sreg]
            rcall L%=_delay_14
            out  %[spdr], __zero_reg__
            clr  r11
            cp   r20, r8
            breq .+2
            inc  r11
            rjmp L%=_begin

        ;
        ;   RENDERING
        ;

        L%=_seek:

            ; seek subroutine
            cbi  %[fxport], %[fxbit]
            ldi  r30, 3
            out  %[spdr], r30
            clr  __zero_reg__
            add  r14, r20
            adc  r15, __zero_reg__
            adc  r16, __zero_reg__
            sbrc r12, 0
            add  r14, r20
            sbrc r12, 0
            adc  r15, __zero_reg__
            sbrc r12, 0
            adc  r16, __zero_reg__
            clr  r11
            cp   r20, r8
            breq .+2
            inc  r11
            lpm
            out  %[spdr], r16
            rcall L%=_delay_17
            out  %[spdr], r15
            rcall L%=_delay_17
            out  %[spdr], r14
            rcall L%=_delay_16
            out  %[spdr], __zero_reg__
            rjmp .+0
            rjmp .+0
            ret
            
        L%=_delay_17:
            nop
        L%=_delay_16:
            rjmp .+0
        L%=_delay_14:
            nop
        L%=_delay_13:
            lpm
        L%=_delay_10:
            lpm
        L%=_delay_7:
            ret

        L%=_begin:

            sbrc r12, 1
            rjmp L%=_begin_erase
            sbrc r12, 2
            rjmp L%=_begin_selfmask
            cp   r17, __zero_reg__
            brlt L%=_top
            tst  r19
            brne L%=_middle_skip_reseek
            rjmp L%=_bottom_dispatch

        L%=_top:

            ; init buf
            subi r26, lo8(-128)
            sbci r27, hi8(-128)
            mov  r21, r8

            ; loop dispatch
            sbrc r12, 0
            rjmp L%=_top_loop_masked

        L%=_top_loop:

            cli
            out  %[spdr], __zero_reg__
            in   r24, %[spdr]
            out  %[sreg], r22
            mul  r24, r5
            ld   r9, X
            and  r9, r7
            or   r9, r1
            st   X+, r9
            rjmp .+0
            dec  r21
            brne L%=_top_loop
            rjmp L%=_top_loop_done

        L%=_top_loop_masked:

            cli
            out  %[spdr], __zero_reg__
            in   r24, %[spdr]
            out  %[sreg], r22
            mul  r24, r5
            movw r24, r0
            rcall L%=_delay_10
            cli
            out  %[spdr], __zero_reg__
            in   r6, %[spdr]
            out  %[sreg], r22
            mul  r6, r5
            movw r6, r0
            ld   r9, X
            com  r7
            and  r9, r7
            or   r9, r25
            st   X+, r9
            dec  r21
            brne L%=_top_loop_masked

        L%=_top_loop_done:

            ; decrement pages, reset buf back
            clr __zero_reg__
            sub  r26, r8
            sbc  r27, __zero_reg__
            dec  r19
            brne L%=_middle
            rjmp L%=_finish

        L%=_middle:

            ; only seek again if necessary
            tst  r11
            breq L%=_middle_skip_reseek
            in   r0, %[spsr]
            sbi  %[fxport], %[fxbit]
            rcall L%=_seek

        L%=_middle_skip_reseek:

            movw r30, r26
            subi r30, lo8(-128)
            sbci r31, hi8(-128)

        L%=_middle_loop_outer:

            mov  r21, r8

            ; loop dispatch
            sbrc r12, 0
            rjmp L%=_middle_loop_inner_masked

            lsr  r21
        1:  brcc L%=_middle_loop_inner
            inc  r21
            ld   r9, X
            rjmp 2f

        L%=_middle_loop_inner:

            ; unrolled twice to meet SPI rate
            in   r24, %[spdr]
            out  %[spdr], __zero_reg__
            mul  r24, r5
            ld   r9, X
            and  r9, r6
            or   r9, r0
            st   X+, r9
            ld   r9, Z
            and  r9, r7
            or   r9, r1
            st   Z+, r9
            ld   r9, X
        2:  in   r24, %[spdr]
            out  %[spdr], __zero_reg__
            mul  r24, r5
            and  r9, r6
            or   r9, r0
            st   X+, r9
            ld   r9, Z
            and  r9, r7
            or   r9, r1
            st   Z+, r9
            nop
            dec  r21
            brne L%=_middle_loop_inner
            rjmp L%=_middle_loop_outer_next

        L%=_middle_loop_inner_masked:

            cli
            out  %[spdr], __zero_reg__
            in   r24, %[spdr]
            out  %[sreg], r22
            mul  r24, r5
            movw r24, r0
            ld   r9, X
            ld   r7, Z
            rcall L%=_delay_7
            in   r6, %[spdr]
            out  %[spdr], __zero_reg__
            mul  r6, r5
            com  r0
            and  r9, r0
            or   r9, r24
            st   X+, r9
            com  r1
            and  r7, r1
            or   r7, r25
            st   Z+, r7
            dec  r21
            brne L%=_middle_loop_inner_masked

        L%=_middle_loop_outer_next:

            ; advance buf to the next page
            clr  __zero_reg__
            add  r26, r4
            adc  r27, __zero_reg__
            dec  r19
            breq 1f
            rjmp L%=_middle
        1:

        L%=_bottom:

            tst  r2
            brne 1f
            rjmp L%=_finish
        1:

            ; seek if needed
            tst  r11
            breq L%=_bottom_dispatch
            in   r0, %[spsr]
            sbi  %[fxport], %[fxbit]
            rcall L%=_seek
            rjmp .+0
            lpm
            
        L%=_bottom_dispatch:

            ; loop dispatch
            sbrc r12, 0
            rjmp L%=_bottom_loop_masked

        L%=_bottom_loop:

            ; write one page from image to buf
            cli
            out  %[spdr], __zero_reg__
            in   r24, %[spdr]
            out  %[sreg], r22
            mul  r24, r5
            ld   r9, X
            and  r9, r6
            or   r9, r0
            st   X+, r9
            rjmp .+0
            dec  r8
            brne L%=_bottom_loop
            rjmp L%=_finish

        L%=_bottom_loop_masked:

            ; write one page from image to buf
            cli
            out  %[spdr], __zero_reg__
            in   r24, %[spdr]
            out  %[sreg], r22
            mul  r24, r5
            movw r24, r0
            rcall L%=_delay_10
            cli
            out  %[spdr], __zero_reg__
            in   r19, %[spdr]
            out  %[sreg], r22
            mul  r19, r5
            mov  r19, r0
            ld   r9, X
            com  r19
            and  r9, r19
            or   r9, r24
            st   X+, r9
            dec  r8
            brne L%=_bottom_loop_masked
            nop
            rjmp L%=_finish

        ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
        ; SELFMASK MODE
        ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

        L%=_begin_selfmask:

            cp   r17, __zero_reg__
            brlt L%=_top_selfmask
            tst  r19
            brne L%=_middle_skip_reseek_selfmask
            rjmp L%=_bottom_loop_selfmask

        L%=_top_selfmask:

            ; init buf
            subi r26, lo8(-128)
            sbci r27, hi8(-128)
            mov  r21, r8

        L%=_top_loop_selfmask:

            cli
            out  %[spdr], __zero_reg__
            in   r24, %[spdr]
            out  %[sreg], r22
            mul  r24, r5
            ld   r9, X
            or   r9, r1
            st   X+, r9
            lpm
            dec  r21
            brne L%=_top_loop_selfmask

        L%=_top_loop_done_selfmask:

            ; decrement pages, reset buf back
            clr __zero_reg__
            sub  r26, r8
            sbc  r27, __zero_reg__
            dec  r19
            brne L%=_middle_selfmask
            rjmp L%=_finish

        L%=_middle_selfmask:

            ; only seek again if necessary
            tst  r11
            breq L%=_middle_skip_reseek_selfmask
            in   r0, %[spsr]
            sbi  %[fxport], %[fxbit]
            rcall L%=_seek
            rjmp .+0
            rjmp .+0

        L%=_middle_skip_reseek_selfmask:

            movw r30, r26
            subi r30, lo8(-128)
            sbci r31, hi8(-128)
            mov  r21, r8

        L%=_middle_loop_inner_selfmask:

            ; write one page from image to buf/buf+128
            in   r24, %[spdr]
            out  %[spdr], __zero_reg__
            mul  r24, r5
            ld   r9, X
            or   r9, r0
            st   X+, r9
            ld   r9, Z
            or   r9, r1
            st   Z+, r9
            nop
            dec  r21
            brne L%=_middle_loop_inner_selfmask

            ; advance buf to the next page
            clr  __zero_reg__
            add  r26, r4
            adc  r27, __zero_reg__
            dec  r19
            brne L%=_middle_selfmask

        L%=_bottom_selfmask:

            tst  r2
            brne 1f
            rjmp L%=_finish
        1:

            ; seek if needed
            tst  r11
            breq L%=_bottom_loop_selfmask
            in   r0, %[spsr]
            sbi  %[fxport], %[fxbit]
            rcall L%=_seek
            rjmp .+0
            rjmp .+0
            lpm        
        
        L%=_bottom_loop_selfmask:

            ; write one page from image to buf
            cli
            out  %[spdr], __zero_reg__
            in   r24, %[spdr]
            out  %[sreg], r22
            mul  r24, r5
            ld   r9, X
            or   r9, r0
            st   X+, r9
            lpm
            dec  r8
            brne L%=_bottom_loop_selfmask
            rjmp L%=_finish

        ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
        ; SELFMASK_ERASE MODE
        ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

        L%=_begin_erase:

            cp   r17, __zero_reg__
            brlt L%=_top_erase
            tst  r19
            brne L%=_middle_skip_reseek_erase
            rjmp L%=_bottom_loop_erase

        L%=_top_erase:

            ; init buf
            subi r26, lo8(-128)
            sbci r27, hi8(-128)
            mov  r21, r8

        L%=_top_loop_erase:

            cli
            out  %[spdr], __zero_reg__
            in   r24, %[spdr]
            out  %[sreg], r22
            mul  r24, r5
            com  r0
            com  r1
            ld   r9, X
            and  r9, r1
            st   X+, r9
            nop
            dec  r21
            brne L%=_top_loop_erase

        L%=_top_loop_done_erase:

            ; decrement pages, reset buf back
            clr __zero_reg__
            sub  r26, r8
            sbc  r27, __zero_reg__
            dec  r19
            brne L%=_middle_erase
            rjmp L%=_finish

        L%=_middle_erase:

            ; only seek again if necessary
            tst  r11
            breq L%=_middle_skip_reseek_erase
            in   r0, %[spsr]
            sbi  %[fxport], %[fxbit]
            rcall L%=_seek
            rjmp .+0
            rjmp .+0

        L%=_middle_skip_reseek_erase:

            movw r30, r26
            subi r30, lo8(-128)
            sbci r31, hi8(-128)
            mov  r21, r8

        L%=_middle_loop_inner_erase:

            ; write one page from image to buf/buf+128
            in   r24, %[spdr]
            out  %[spdr], __zero_reg__
            mul  r24, r5
            com  r0
            com  r1
            ld   r9, X
            and  r9, r0
            st   X+, r9
            ld   r9, Z
            and  r9, r1
            st   Z+, r9
            dec  r21
            brne L%=_middle_loop_inner_erase

            ; advance buf to the next page
            clr  __zero_reg__
            add  r26, r4
            adc  r27, __zero_reg__
            dec  r19
            brne L%=_middle_erase

        L%=_bottom_erase:

            tst  r2
            brne 1f
            rjmp L%=_finish
        1:

            ; seek if needed
            tst  r11
            breq L%=_bottom_loop_pre_erase
            in   r0, %[spsr]
            sbi  %[fxport], %[fxbit]
            rcall L%=_seek
            rjmp .+0

        L%=_bottom_loop_pre_erase:
        
            rjmp .+0
        
        L%=_bottom_loop_erase:

            ; write one page from image to buf
            cli
            out  %[spdr], __zero_reg__
            in   r24, %[spdr]
            out  %[sreg], r22
            mul  r24, r5
            com  r0
            com  r1
            ld   r9, X
            and  r9, r0
            st   X+, r9
            nop
            dec  r8
            brne L%=_bottom_loop_erase

        L%=_finish:

            clr  __zero_reg__
            sbi  %[fxport], %[fxbit]
            in   r0, %[spsr]
            
        L%=_end:

            pop  r17
            pop  r16
            pop  r15
            pop  r14
            ; pop  r13 ; unmodified
            ; pop  r12 ; unmodified
            pop  r11
            ; pop  r10 ; unmodified
            pop  r9
            pop  r8
            pop  r7
            pop  r6
            pop  r5
            pop  r4
            pop  r3
            pop  r2
            
        L%=_end_postpop:
            
            ret

        )ASM"
        
        :
        : [sBuffer]    "i"   (Arduboy2Base::sBuffer)
        , [fxport]     "I"   (_SFR_IO_ADDR(FX_PORT))
        , [fxbit]      "I"   (FX_BIT)
        , [spdr]       "I"   (_SFR_IO_ADDR(SPDR))
        , [spsr]       "I"   (_SFR_IO_ADDR(SPSR))
        , [sreg]       "I"   (_SFR_IO_ADDR(SREG))
        , [page]       "i"   (&FX::programDataPage)
                           
        );
}

// from Mr. Blinky's ArduboyFX library
[[gnu::always_inline]]
static uint8_t SpritesABC_bitShiftLeftMaskUInt8(uint8_t bit)
{
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
}

void SpritesABC::fillRect(int16_t x, int16_t y, uint8_t w, uint8_t h, uint8_t color)
{
    if(x >= 128) return;
    if(y >= 64)  return;
    if(x + w <= 0) return;
    if(y + h <= 0) return;
    if(w == 0 || h == 0) return;
    
    if(color & 1) color = 0xff;

    // clip coords
    uint8_t xc = x;
    uint8_t yc = y;

    // clip
    if(y < 0)
        h += (int8_t)y, yc = 0;
    if(x < 0)
        w += (int8_t)x, xc = 0;
    if(h >= uint8_t(64 - yc))
        h = 64 - yc;
    if(w >= uint8_t(128 - xc))
        w = 128 - xc;
    uint8_t y1 = yc + h;

    uint8_t c0 = SpritesABC_bitShiftLeftMaskUInt8(yc); // 11100000
    uint8_t m1 = SpritesABC_bitShiftLeftMaskUInt8(y1); // 11000000
    uint8_t m0 = ~c0; // 00011111
    uint8_t c1 = ~m1; // 00111111

    uint8_t r0 = yc;
    uint8_t r1 = y1 - 1;
    asm volatile(
        "lsr %[r0]\n"
        "lsr %[r0]\n"
        "lsr %[r0]\n"
        "lsr %[r1]\n"
        "lsr %[r1]\n"
        "lsr %[r1]\n"
        : [r0] "+&r" (r0),
          [r1] "+&r" (r1));

    uint8_t* buf = Arduboy2Base::sBuffer;
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

    uint8_t rows = r1 - r0; // middle rows + 1
    uint8_t bot = c1;
    if(c0 & 1) ++rows; // no top fragment
    if(m1 & 1) ++rows; // no bottom fragment
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

        L%=_middle_loop:
            mov  %[col], %[w]
            andi %[col], 7
            brne 3f
            mov  %[col], %[w]
            
        1:  st   %a[buf]+, %[color]
            st   %a[buf]+, %[color]
            st   %a[buf]+, %[color]
            st   %a[buf]+, %[color]
            st   %a[buf]+, %[color]
            st   %a[buf]+, %[color]
            st   %a[buf]+, %[color]
            st   %a[buf]+, %[color]
            subi %[col], 8
            brne 1b
            
        2:  add  %A[buf], %[buf_adv]
            adc  %B[buf], __zero_reg__
            dec  %[rows]
            brne L%=_middle_loop
            rjmp L%=_bottom

        3:  st   %a[buf]+, %[color]
            dec  %[col]
            brne 3b
            mov  %[col], %[w]
            andi %[col], 0xf8
            brne 1b
            add  %A[buf], %[buf_adv]
            adc  %B[buf], __zero_reg__
            dec  %[rows]
            brne L%=_middle_loop

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
        [w]       "+&d" (w),
        [rows]    "+&r" (rows),
        [col]     "=&d" (col)
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
