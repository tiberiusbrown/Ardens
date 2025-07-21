#pragma once

#include <avr/io.h>

#include <stdint.h>

// profile f and return elapsed cycles
// note: uses timer3
template<class F>
[[gnu::noinline]]
inline uint32_t profile(F&& f)
{
    uint8_t sreg;
    __uint24 cycles;
    asm volatile(R"(
            sts %[timsk1], __zero_reg__
            sts %[tccr1a], __zero_reg__
            sts %[timsk3], __zero_reg__
            sts %[tccr3a], __zero_reg__
            ldi %[s], 0x04
            sts %[tccr1b], %[s]
            ldi %[s], 0x01
            sts %[tccr3b], %[s]
            in  %[s], %[sreg]
            cli
            sts %[tcnt1]+1, __zero_reg__
            sts %[tcnt1]+0, __zero_reg__
            sts %[tcnt3]+1, __zero_reg__
            sts %[tcnt3]+0, __zero_reg__
        )"
        : [s]      "=&d" (sreg)
        : [sreg]   "I"   (_SFR_IO_ADDR(SREG))
        , [ocr1a]  ""    (&OCR1A)
        , [tcnt1]  ""    (&TCNT1)
        , [tccr1a] ""    (&TCCR1A)
        , [tccr1b] ""    (&TCCR1B)
        , [timsk1] ""    (&TIMSK1)
        , [tcnt3]  ""    (&TCNT3)
        , [tccr3a] ""    (&TCCR3A)
        , [tccr3b] ""    (&TCCR3B)
        , [timsk3] ""    (&TIMSK3)
    );
    f();
    asm volatile(R"(
            lds  r0, %[tcnt1]+0
            lds  %C[cycles], %[tcnt1]+1
            lds  %A[cycles], %[tcnt3]+0
            lds  %B[cycles], %[tcnt3]+1
            sts  %[tccr1b], __zero_reg__
            sts  %[tccr3b], __zero_reg__
            out  %[sreg], %[s]
        )"
        : [cycles] "=&r" (cycles)
        : [s]      "r"   (sreg)
        , [sreg]   "I"   (_SFR_IO_ADDR(SREG))
        , [tcnt1]  ""    (&TCNT1)
        , [tccr1b] ""    (&TCCR1B)
        , [tcnt3]  ""    (&TCNT3)
        , [tccr3b] ""    (&TCCR3B)
    );
    return cycles - 5;
}
