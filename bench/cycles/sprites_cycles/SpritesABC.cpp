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
        uint8_t  reseek       [T flag]
        uint16_t shift_mask   r6
        uint8_t  cols         r8
        uint8_t  mode         r12
        uint24_t image        r14
        int8_t   page_start   r17 (reused for shift_coef)
        uint8_t  h            r18 (reused for buf_data)
        uint8_t  pages        r19
        uint8_t  w            r20
        uint8_t  count        r21
        int16_t  y            r22 (reused for sreg and bottom)
        bool     bottom       r23 bit 0
        bool     top          r23 bit 1
        int16_t  x            r24 (reused in render loops and for buf_adv)
        uint8_t* buf          r26
        uint8_t* bufn         r30

        INPUT

            r24:r25      x
            r22:r23      y
            r20          w
            r18          h
            r14:r15:r16  image
            r12          mode

        OUTPUT

            T flag       reseek
            r4           buf_adv
            r6:r7        [reserved for use in render loops]
            r8           cols
            r12          mode
            r14:r15:r16  image - w (w readded in first seek)
            r17          shift_coef
            r18          [reserved for use in render loops]
            r19          pages
            r20          w (needed for reseek method)
            r21          [reserved for render loop counter]
            r22 (w/ T)   sreg
            r23 bit 0    bottom
            r23 bit 1    top (page_start < 0)
            r24          [reserved for use in render loops]
            r25
            r26:r27      buf
            r30:r31      bufn

        PSEUDOCODE (unfinished)

            image += data_page * 256
            
            pages = h >> 3
            page_start = y >> 3;
            if page_start < -1:
                page_stage = -page_start - 1   [~page_start]
                pages -= page_start
                if mode & 1:
                    page_start <<= 1
                image += page_start * w
                page_start = -1

            cols = w
            if x < 0:
                cols += x
                if mode & 1:
                    x <<= 1
                image -= x
                x = 0
            
            buf = sBuffer + page_start * 128 + x

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
    
            ; begin initial seek
            cbi  %[fxport], %[fxbit]
            ldi  r21, 3
            out  %[spdr], r21
    
            ; push r2  ; unmodified
            ; push r3  ; unmodified
            ; push r4  ; unmodified
            ; push r5  ; unmodified
            push r6
            push r7
            push r8
            ; push r9  ; unmodified
            ; push r10 ; unmodified
            ; push r11 ; unmodified
            ; push r12 ; unmodified
            ; push r13 ; unmodified
            push r14
            push r15
            push r16
            push r17
        
            clr  r6
    
            mov  r19, r18
            lsr  r19
            lsr  r19
            lsr  r19
            
            ; page_start
            mov  r17, r22
            asr  r23
            ror  r17
            asr  r17
            asr  r17
            
            ; clip against top edge
            cpi  r17, 0xff
            brge 1f
            com  r17
            sub  r19, r17
            sbrc r12, 0
            lsl  r17
            mul  r17, r20
            add  r14, r0
            adc  r15, r1
            adc  r16, r6
            ldi  r17, 0xff
        1:
        
            lds r0, %[page]+0
            add r15, r0
            lds r0, %[page]+1
            adc r16, r0
        
            ; clip against left edge
            mov  r8, r20
            sbrs r25, 7
            rjmp 2f
            add  r8, r24
            sbrs r12, 0
            rjmp 1f
            lsl  r24
            rol  r25
        1:  sub  r14, r24
            sbc  r15, r25
            sbc  r16, r6
            sbrc r25, 7
            inc  r16
            clr  r24
        2:
        
            ; continue initial seek
            out  %[spdr], r16
        
            ; compute buffer start address
            ldi  r26, lo8(%[sBuffer])
            ldi  r27, hi8(%[sBuffer])
            ldi  r21, 128
            mulsu r17, r21
            add  r0, r24
            add  r26, r0
            adc  r27, r1
            
            ; clip against right edge
            sub  r21, r24
            cp   r8, r21
            brlo 1f
            mov  r8, r21
        1:
        
            ; flag register
            clr  r23

            ; clip against bottom edge
            ldi  r30, 7
            sub  r30, r17

            ; top flag: r23[1] = (page_start < 0)
            bst  r17, 7
            bld  r23, 1
            
            ; continue initial seek
            out  %[spdr], r15
            
            cp   r30, r19
            brge 1f
            mov  r19, r30
            ori  r23, (1<<0)
        1:
        
            ldi  r25, 128
            sub  r25, r8
            
            ; precompute vertical shift coef and mask
            ldi  r17, 1
            sbrc r22, 1
            ldi  r17, 4
            sbrc r22, 0
            lsl  r17
            sbrc r22, 2
            swap r17

            ldi  r30, 0xff
            mul  r30, r17
            movw r6, r0

            com  r6
            
            ; continue initial seek
            out  %[spdr], r14
        
            com  r7
            clr  __zero_reg__
            rcall L%=_delay_14

            ; continue initial seek
            out  %[spdr], __zero_reg__

            ; reseek flag in T
            clt
            cpse r20, r8
            set
            in   r22, %[sreg]

        ;
        ;   RENDERING
        ;

        L%=_begin:

            sbrc r12, 1
            rjmp L%=_begin_erase
            sbrc r12, 2
            rjmp L%=_begin_selfmask
            sbrc r23, 1
            rjmp L%=_top
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
            mul  r24, r17
            ld   r18, X
            and  r18, r7
            or   r18, r1
            st   X+, r18
            rjmp .+0
            dec  r21
            brne L%=_top_loop
            rjmp L%=_top_loop_done

        L%=_top_loop_masked:

            cli
            out  %[spdr], __zero_reg__
            in   r0, %[spdr]
            out  %[sreg], r22
            mul  r0, r17
            mov  r24, r1
            rcall L%=_delay_10
            cli
            out  %[spdr], __zero_reg__
            in   r0, %[spdr]
            out  %[sreg], r22
            mul  r0, r17
            mov  r7, r1
            ld   r18, X
            com  r7
            and  r18, r7
            or   r18, r24
            st   X+, r18
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
            brtc L%=_middle_skip_reseek
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
            ld   r18, X
            rjmp 2f

        L%=_middle_loop_inner:

            ; unrolled twice to meet SPI rate
            in   r24, %[spdr]
            out  %[spdr], __zero_reg__
            mul  r24, r17
            ld   r18, X
            and  r18, r6
            or   r18, r0
            st   X+, r18
            ld   r18, Z
            and  r18, r7
            or   r18, r1
            st   Z+, r18
            ld   r18, X
        2:  in   r24, %[spdr]
            out  %[spdr], __zero_reg__
            mul  r24, r17
            and  r18, r6
            or   r18, r0
            st   X+, r18
            ld   r18, Z
            and  r18, r7
            or   r18, r1
            st   Z+, r18
            nop
            dec  r21
            brne L%=_middle_loop_inner
            rjmp L%=_middle_loop_outer_next

        L%=_middle_loop_inner_masked:

            cli
            out  %[spdr], __zero_reg__
            in   r0, %[spdr]
            out  %[sreg], r22
            mul  r0, r17
            movw r6, r0
            ld   r18, X
            ld   r24, Z
            rcall L%=_delay_7
            in   r0, %[spdr]
            out  %[spdr], __zero_reg__
            mul  r0, r17
            com  r0
            and  r18, r0
            or   r18, r6
            st   X+, r18
            com  r1
            and  r24, r1
            or   r24, r7
            st   Z+, r24
            dec  r21
            brne L%=_middle_loop_inner_masked

        L%=_middle_loop_outer_next:

            ; advance buf to the next page
            clr  __zero_reg__
            add  r26, r25
            adc  r27, __zero_reg__
            dec  r19
            breq 1f
            rjmp L%=_middle
        1:

        L%=_bottom:

            sbrs r23, 0
            rjmp L%=_finish

            ; seek if needed
            brtc L%=_bottom_dispatch
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
            mul  r24, r17
            ld   r18, X
            and  r18, r6
            or   r18, r0
            st   X+, r18
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
            mul  r24, r17
            movw r24, r0
            rcall L%=_delay_10
            cli
            out  %[spdr], __zero_reg__
            in   r19, %[spdr]
            out  %[sreg], r22
            mul  r19, r17
            mov  r19, r0
            ld   r18, X
            com  r19
            and  r18, r19
            or   r18, r24
            st   X+, r18
            dec  r8
            brne L%=_bottom_loop_masked
            nop
            rjmp L%=_finish

        ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
        ; SELFMASK MODE
        ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

        L%=_begin_selfmask:

            sbrc r23, 1
            rjmp L%=_top_selfmask
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
            mul  r24, r17
            ld   r18, X
            or   r18, r1
            st   X+, r18
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
            brtc L%=_middle_skip_reseek_selfmask
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
            mul  r24, r17
            ld   r18, X
            or   r18, r0
            st   X+, r18
            ld   r18, Z
            or   r18, r1
            st   Z+, r18
            nop
            dec  r21
            brne L%=_middle_loop_inner_selfmask

            ; advance buf to the next page
            clr  __zero_reg__
            add  r26, r25
            adc  r27, __zero_reg__
            dec  r19
            brne L%=_middle_selfmask

        L%=_bottom_selfmask:

            sbrs r23, 0
            rjmp L%=_finish

            ; seek if needed
            brtc L%=_bottom_loop_selfmask
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
            mul  r24, r17
            ld   r18, X
            or   r18, r0
            st   X+, r18
            lpm
            dec  r8
            brne L%=_bottom_loop_selfmask
            rjmp L%=_finish

        ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
        ; SELFMASK_ERASE MODE
        ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

        L%=_begin_erase:

            sbrc r23, 1
            rjmp L%=_top_erase
            tst  r19
            brne L%=_middle_skip_reseek_erase
            rjmp L%=_bottom_loop_erase

        L%=_top_erase:

            ; init buf
            subi r26, lo8(-128)
            sbci r27, hi8(-128)
            mov  r21, r8
            rjmp .+0

        L%=_top_loop_erase:

            cli
            out  %[spdr], __zero_reg__
            in   r24, %[spdr]
            out  %[sreg], r22
            mul  r24, r17
            com  r0
            com  r1
            ld   r18, X
            and  r18, r1
            st   X+, r18
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
            brtc L%=_middle_skip_reseek_erase
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
            mul  r24, r17
            com  r0
            com  r1
            ld   r18, X
            and  r18, r0
            st   X+, r18
            ld   r18, Z
            and  r18, r1
            st   Z+, r18
            dec  r21
            brne L%=_middle_loop_inner_erase

            ; advance buf to the next page
            clr  __zero_reg__
            add  r26, r25
            adc  r27, __zero_reg__
            dec  r19
            brne L%=_middle_erase

        L%=_bottom_erase:

            sbrs r23, 0
            rjmp L%=_finish

            ; seek if needed
            brtc L%=_bottom_loop_pre_erase
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
            mul  r24, r17
            com  r0
            com  r1
            ld   r18, X
            and  r18, r0
            st   X+, r18
            nop
            dec  r8
            brne L%=_bottom_loop_erase

        L%=_finish:

            clr  __zero_reg__
            sbi  %[fxport], %[fxbit]
            
        L%=_end:

            pop  r17
            pop  r16
            pop  r15
            pop  r14
            ; pop  r13 ; unmodified
            ; pop  r12 ; unmodified
            ; pop  r11 ; unmodified
            ; pop  r10 ; unmodified
            ; pop  r9  ; unmodified
            pop  r8
            pop  r7
            pop  r6
            ; pop  r5  ; unmodified
            ; pop  r4  ; unmodified
            ; pop  r3  ; unmodified
            ; pop  r2  ; unmodified
            
        L%=_end_postpop:
            
            ret

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
            rcall L%=_delay_7
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

void SpritesABC::fillRect(int16_t x, int16_t y, uint8_t w, uint8_t h, uint8_t color)
{
    if(x >= 128) return;
    if(y >= 64)  return;
    if(x + w <= 0) return;
    if(y + h <= 0) return;
    if(w == 0 || h == 0) return;

#if 1
    uint8_t t;
    asm volatile(R"(
            sbrs %B[y], 7
            rjmp 1f
            add  %[h], %A[y]
            clr  %A[y]
        1:
            sbrs %B[x], 7
            rjmp 1f
            add  %[w], %A[x]
            clr  %A[x]
        1:
            ldi  %[t], 64
            sub  %[t], %A[y]
            cp   %[h], %[t]
            brlo 1f
            mov  %[h], %[t]
        1:
            ldi  %[t], 128
            sub  %[t], %A[x]
            cp   %[w], %[t]
            brlo 1f
            mov  %[w], %[t]
        1:
        )"
        : [t] "=&d" (t)
        , [x] "+&r" (x)
        , [y] "+&r" (y)
        , [w] "+&r" (w)
        , [h] "+&r" (h)
    );

    fillRect_clipped((uint8_t)x, (uint8_t)y, w, h, color);
#else
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

    fillRect_clipped(xc, yc, w, h, color);
#endif

}

void SpritesABC::fillRect_clipped(uint8_t xc, uint8_t yc, uint8_t w, uint8_t h, uint8_t color)
{
    uint8_t r0, m1, c0;
    uint8_t* buf;
    uint8_t buf_adv;
    uint8_t col;
    uint8_t t;

    asm volatile(R"(

            add  %[r1], %[r0]

            ldi  %[c0], 1
            sbrc %[r0], 1
            ldi  %[c0], 4
            sbrc %[r0], 0
            lsl  %[c0]
            sbrc %[r0], 2
            swap %[c0]
            neg  %[c0]

            ldi  %[m1], 1
            sbrc %[r1], 1
            ldi  %[m1], 4
            sbrc %[r1], 0
            lsl  %[m1]
            sbrc %[r1], 2
            swap %[m1]
            neg  %[m1]

            dec  %[r1]

            lsr  %[r0]
            lsr  %[r0]
            lsr  %[r0]
            lsr  %[r1]
            lsr  %[r1]
            lsr  %[r1]

            ldi  %[buf_adv], 128
            mul  %[r0], %[buf_adv]
            movw %A[buf], r0
            clr  __zero_reg__
            subi %A[buf], lo8(-(%[sBuffer]))
            sbci %B[buf], hi8(-(%[sBuffer]))
            add  %A[buf], %[x]
            adc  %B[buf], __zero_reg__
            sub  %[buf_adv], %[w]

            sub  %[r1], %[r0]
            sbrc %[c0], 0
            inc  %[r1]
            sbrc %[m1], 0
            inc  %[r1]
            sbrc %[color], 0
            ldi  %[color], 0xff
        )"
        : [buf]     "=&e" (buf)
        , [buf_adv] "=&d" (buf_adv)
        , [r0]      "+&r" (yc)
        , [r1]      "+&r" (h)
        , [color]   "+&d" (color)
        , [c0]      "=&d" (c0)
        , [m1]      "=&d" (m1)
        : [x]       "r"   (xc)
        , [w]       "r"   (w)
        , [sBuffer] ""    (Arduboy2Base::sBuffer)
        );

    asm volatile(R"(
            mov  %[t], %[c0]
            com  %[t]
            cpse %[rows], __zero_reg__
            rjmp L%=_top
            mov  r0, %[m1]
            com  r0
            or   %[m1], %[t]
            mov  %[t], r0
            and  %[t], %[c0]
            and  %[t], %[color]
            rjmp L%=_bottom_loop

        L%=_top:
            ; Z flag from com t still set
            breq L%=_middle
            and  %[c0], %[color]
            mov  %[col], %[w]

        L%=_top_loop:
            ld   __tmp_reg__, %a[buf]
            and  __tmp_reg__, %[t]
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
            mov  %[t], %[m1]
            com  %[t]
            breq L%=_finish
            and  %[t], %[color]

        L%=_bottom_loop:
            ld   __tmp_reg__, %a[buf]
            and  __tmp_reg__, %[m1]
            or   __tmp_reg__, %[t]
            st   %a[buf]+, __tmp_reg__
            dec  %[w]
            brne L%=_bottom_loop

        L%=_finish:
        )"
        : [buf]     "+&e" (buf)
        , [w]       "+&r" (w)
        , [rows]    "+&r" (h)
        , [col]     "=&d" (col)
        , [t]       "=&r" (t)
        : [buf_adv] "r"   (buf_adv)
        , [color]   "r"   (color)
        , [m1]      "r"   (m1)
        , [c0]      "r"   (c0)
        );
}
