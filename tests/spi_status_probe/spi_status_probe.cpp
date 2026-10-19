// ATmega32U4 / 16 MHz Arduboy SPI acknowledgment-priority measurement.
// Measurements finish before the OLED is touched. No flash device is selected.
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/wdt.h>
#include <util/delay.h>
#include <stdint.h>

struct sample_t
{
    uint8_t before, after, late, access_rx, late_rx;
};

volatile sample_t spi_probe_results[18];
volatile uint8_t spi_probe_done;

// Kind 0: old SPIF, current transfer starts at 18, SPSR read at 19.
// Kind 1: WCOL only, current transfer starts at 0, collision at 6, SPSR at 7.
// Kind 2: no old flags, current transfer starts at 0, SPSR read at 7.
// Gap is measured from the current transfer's OUT to the measured SPDR access.
template<unsigned Kind, bool Write, unsigned Gap>
static void measure(unsigned index)
{
    // Finish any previous transfer, then acknowledge its flags before this run.
    asm volatile(".rept 40\n\tnop\n\t.endr" ::: "memory");
    (void)SPSR;
    (void)SPDR;
    uint8_t before, after, late, access_rx = 0xff, late_rx;
    asm volatile(
        "out %[spdr], %[first]\n\t"
        ".if %[kind] == 0\n\t"
        ".rept 17\n\tnop\n\t.endr\n\t"
        "out %[spdr], %[second]\n\t"
        "in %[before], %[spsr]\n\t"
        ".rept %[gap] - 2\n\tnop\n\t.endr\n\t"
        ".else\n\t"
        ".rept 5\n\tnop\n\t.endr\n\t"
        ".if %[kind] == 1\n\t"
        "out %[spdr], %[second]\n\t"
        ".else\n\tnop\n\t.endif\n\t"
        "in %[before], %[spsr]\n\t"
        ".rept %[gap] - 8\n\tnop\n\t.endr\n\t"
        ".endif\n\t"
        ".if %[write]\n\t"
        "out %[spdr], %[third]\n\t"
        ".else\n\t"
        "in %[access_rx], %[spdr]\n\t"
        ".endif\n\t"
        "in %[after], %[spsr]\n\t"
        ".rept 32\n\tnop\n\t.endr\n\t"
        "in %[late], %[spsr]\n\t"
        "in %[late_rx], %[spdr]\n\t"
        : [before] "=&r" (before), [after] "=&r" (after),
          [late] "=&r" (late), [access_rx] "+&r" (access_rx),
          [late_rx] "=&r" (late_rx)
        : [spdr] "I" (_SFR_IO_ADDR(SPDR)), [spsr] "I" (_SFR_IO_ADDR(SPSR)),
          [kind] "n" (Kind), [gap] "n" (Gap), [write] "n" (Write),
          [first] "r" (uint8_t(0x5a)), [second] "r" (uint8_t(0x96)),
          [third] "r" (uint8_t(0xa5))
        : "memory");
    spi_probe_results[index].before = before;
    spi_probe_results[index].after = after;
    spi_probe_results[index].late = late;
    spi_probe_results[index].access_rx = access_rx;
    spi_probe_results[index].late_rx = late_rx;
}

template<unsigned Kind>
static void group()
{
    measure<Kind, false, 16>(Kind * 6 + 0);
    measure<Kind, false, 17>(Kind * 6 + 1);
    measure<Kind, false, 18>(Kind * 6 + 2);
    measure<Kind, true, 16>(Kind * 6 + 3);
    measure<Kind, true, 17>(Kind * 6 + 4);
    measure<Kind, true, 18>(Kind * 6 + 5);
}

static void send(uint8_t value)
{
    SPDR = value;
    while(!(SPSR & _BV(SPIF))) {}
    (void)SPDR;
}

static uint8_t const hex_font[16][5] PROGMEM = {
    { 0x3e, 0x51, 0x49, 0x45, 0x3e }, { 0, 0x42, 0x7f, 0x40, 0 },
    { 0x42, 0x61, 0x51, 0x49, 0x46 }, { 0x21, 0x41, 0x45, 0x4b, 0x31 },
    { 0x18, 0x14, 0x12, 0x7f, 0x10 }, { 0x27, 0x45, 0x45, 0x45, 0x39 },
    { 0x3c, 0x4a, 0x49, 0x49, 0x30 }, { 1, 0x71, 9, 5, 3 },
    { 0x36, 0x49, 0x49, 0x49, 0x36 }, { 6, 0x49, 0x49, 0x29, 0x1e },
    { 0x7e, 0x11, 0x11, 0x11, 0x7e }, { 0x7f, 0x49, 0x49, 0x49, 0x36 },
    { 0x3e, 0x41, 0x41, 0x41, 0x22 }, { 0x7f, 0x41, 0x41, 0x22, 0x1c },
    { 0x7f, 0x49, 0x49, 0x49, 0x41 }, { 0x7f, 9, 9, 9, 1 },
};

static void digit(uint8_t value)
{
    for(uint8_t col = 0; col < 5; ++col)
        send(pgm_read_byte(&hex_font[value & 15][col]));
    send(0);
}

static void byte(uint8_t value)
{
    digit(value >> 4);
    digit(value);
    send(0);
    send(0);
}

static void show(uint8_t page)
{
    PORTD &= ~_BV(6); // OLED CS
    for(uint8_t row = 0; row < 8; ++row)
    {
        PORTD &= ~_BV(4);
        send(0xb0 | row);
        send(0);
        send(0x10);
        PORTD |= _BV(4);
        if(row == 0)
        {
            byte(0x53); byte(0x50); byte(0x01); byte(page); byte(0); byte(0);
        }
        else if(row <= 6)
        {
            uint8_t index = page * 6 + row - 1;
            byte(index);
            byte(spi_probe_results[index].before);
            byte(spi_probe_results[index].after);
            byte(spi_probe_results[index].late);
            byte(spi_probe_results[index].access_rx);
            byte(spi_probe_results[index].late_rx);
        }
        else
            for(uint8_t col = 0; col < 84; ++col) send(0);
        for(uint8_t col = 84; col < 128; ++col) send(0);
    }
    PORTD |= _BV(6);
}

int main()
{
    cli();
    MCUSR = 0;
    wdt_disable();
    CLKPR = _BV(CLKPCE);
    CLKPR = 0;
    UDCON = _BV(DETACH);
    USBCON = 0;
    // Standard FX (D1/D2) and FX-C (E2) flash selects remain high throughout.
    PORTD |= _BV(1) | _BV(2) | _BV(4) | _BV(6) | _BV(7);
    DDRD |= _BV(1) | _BV(2) | _BV(4) | _BV(6) | _BV(7);
    PORTE |= _BV(2) | _BV(6);
    DDRE = (DDRE | _BV(2)) & ~_BV(6);
    PORTB |= _BV(0) | _BV(3) | _BV(4) | _BV(5) | _BV(6) | _BV(7);
    DDRB = (DDRB | _BV(0) | _BV(1) | _BV(2) | _BV(5) | _BV(6) | _BV(7)) &
        ~(_BV(3) | _BV(4));
    SPCR = _BV(SPE) | _BV(MSTR);
    SPSR = _BV(SPI2X);
    group<0>();
    group<1>();
    group<2>();
    spi_probe_done = 1;
    asm volatile(".rept 40\n\tnop\n\t.endr" ::: "memory");
    (void)SPSR;
    (void)SPDR;
    PORTD &= ~_BV(7);
    _delay_ms(5);
    PORTD |= _BV(7);
    _delay_ms(5);
    uint8_t const commands[] = {
        0xae, 0xd5, 0xf0, 0xa8, 0x3f, 0xd3, 0, 0x40, 0x8d, 0x14,
        0x20, 2, 0xa1, 0xc8, 0xda, 0x12, 0x81, 0x7f, 0xd9, 0xf1,
        0xdb, 0x40, 0xa4, 0xa6, 0xaf,
    };
    PORTD &= ~(_BV(6) | _BV(4));
    for(uint8_t value : commands) send(value);
    PORTD |= _BV(6);
    uint8_t page = 0;
    show(page);
    for(;;)
    {
        if(!(PINE & _BV(6))) // A: next page
        {
            page = (page + 1) % 3;
            show(page);
            while(!(PINE & _BV(6))) {}
            _delay_ms(100);
        }
        if(!(PINB & _BV(4))) // B: previous page
        {
            page = (page + 2) % 3;
            show(page);
            while(!(PINB & _BV(4))) {}
            _delay_ms(100);
        }
    }
}
