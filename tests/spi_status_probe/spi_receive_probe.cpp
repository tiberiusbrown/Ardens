// Read-only FX-C JEDEC-ID probe. Reuse the status probe's OLED/font helpers.
#define main unused_status_probe_main
#include "spi_status_probe.cpp"
#undef main

static uint8_t polled(uint8_t value)
{
    SPDR = value;
    while(!(SPSR & _BV(SPIF))) {}
    return SPDR;
}

static void begin_id()
{
    PORTE |= _BV(2);
    asm volatile(".rept 40\n\tnop\n\t.endr" ::: "memory");
    (void)SPSR;
    (void)SPDR;
    PORTE &= ~_BV(2); // FX-C chip select
    (void)polled(0x9f); // read JEDEC ID; no write/erase commands are issued
}

static void save(unsigned index, uint8_t first, uint8_t second, uint8_t third,
    uint8_t after, uint8_t late)
{
    PORTE |= _BV(2);
    spi_probe_results[index].before = first;
    spi_probe_results[index].after = second;
    spi_probe_results[index].late = third;
    spi_probe_results[index].access_rx = after;
    spi_probe_results[index].late_rx = late;
}

template<unsigned Gap>
static void write_gap(unsigned index)
{
    begin_id();
    uint8_t first, second, after, late;
    asm volatile(
        "out %[spdr], __zero_reg__\n\t"
        ".rept %[gap] - 1\n\tnop\n\t.endr\n\t"
        "out %[spdr], __zero_reg__\n\t"
        "in %[first], %[spdr]\n\t"
        "in %[after], %[spsr]\n\t"
        ".rept 40\n\tnop\n\t.endr\n\t"
        "in %[second], %[spdr]\n\t"
        "in %[late], %[spsr]\n\t"
        : [first] "=&r" (first), [second] "=&r" (second),
          [after] "=&r" (after), [late] "=&r" (late)
        : [gap] "n" (Gap), [spdr] "I" (_SFR_IO_ADDR(SPDR)),
          [spsr] "I" (_SFR_IO_ADDR(SPSR))
        : "memory");
    uint8_t third = polled(0);
    save(index, first, second, third, after, late);
}

template<unsigned Gap>
static void read_gap(unsigned index)
{
    begin_id();
    uint8_t first, second, after, late;
    asm volatile(
        "out %[spdr], __zero_reg__\n\t"
        ".rept %[gap] - 1\n\tnop\n\t.endr\n\t"
        "in %[first], %[spdr]\n\t"
        "in %[after], %[spsr]\n\t"
        ".rept 40\n\tnop\n\t.endr\n\t"
        "in %[second], %[spdr]\n\t"
        "in %[late], %[spsr]\n\t"
        : [first] "=&r" (first), [second] "=&r" (second),
          [after] "=&r" (after), [late] "=&r" (late)
        : [gap] "n" (Gap), [spdr] "I" (_SFR_IO_ADDR(SPDR)),
          [spsr] "I" (_SFR_IO_ADDR(SPSR))
        : "memory");
    uint8_t third = polled(0);
    save(index, first, second, third, after, late);
}

template<unsigned Gap>
static void stream(unsigned index)
{
    begin_id();
    uint8_t first, second, third, after, late;
    asm volatile(
        "out %[spdr], __zero_reg__\n\t"
        ".rept %[gap] - 1\n\tnop\n\t.endr\n\t"
        "out %[spdr], __zero_reg__\n\t"
        "in %[first], %[spdr]\n\t"
        ".rept %[gap] - 2\n\tnop\n\t.endr\n\t"
        "out %[spdr], __zero_reg__\n\t"
        "in %[second], %[spdr]\n\t"
        ".rept %[gap] - 2\n\tnop\n\t.endr\n\t"
        "out %[spdr], __zero_reg__\n\t"
        "in %[third], %[spdr]\n\t"
        "in %[after], %[spsr]\n\t"
        ".rept 40\n\tnop\n\t.endr\n\t"
        "in %[late], %[spsr]\n\t"
        : [first] "=&r" (first), [second] "=&r" (second), [third] "=&r" (third),
          [after] "=&r" (after), [late] "=&r" (late)
        : [gap] "n" (Gap), [spdr] "I" (_SFR_IO_ADDR(SPDR)),
          [spsr] "I" (_SFR_IO_ADDR(SPSR))
        : "memory");
    save(index, first, second, third, after, late);
}

static void show_receive(uint8_t page)
{
    PORTD &= ~_BV(6);
    for(uint8_t row = 0; row < 8; ++row)
    {
        PORTD &= ~_BV(4);
        send(0xb0 | row); send(0); send(0x10);
        PORTD |= _BV(4);
        if(row == 0)
        {
            byte(0x53); byte(0x50); byte(2); byte(page); byte(0); byte(0);
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
        else for(uint8_t col = 0; col < 84; ++col) send(0);
        for(uint8_t col = 84; col < 128; ++col) send(0);
    }
    PORTD |= _BV(6);
}

int main()
{
    cli(); MCUSR = 0; wdt_disable(); CLKPR = _BV(CLKPCE); CLKPR = 0;
    UDCON = _BV(DETACH); USBCON = 0;
    PORTD |= _BV(1) | _BV(2) | _BV(4) | _BV(6) | _BV(7);
    DDRD |= _BV(1) | _BV(2) | _BV(4) | _BV(6) | _BV(7);
    PORTE |= _BV(2) | _BV(6); DDRE = (DDRE | _BV(2)) & ~_BV(6);
    PORTB |= _BV(0) | _BV(3) | _BV(4) | _BV(5) | _BV(6) | _BV(7);
    DDRB = (DDRB | _BV(0) | _BV(1) | _BV(2) | _BV(5) | _BV(6) | _BV(7)) &
        ~(_BV(3) | _BV(4));
    SPCR = _BV(SPE) | _BV(MSTR); SPSR = _BV(SPI2X);
    // Release flash from power-down, then deselect before the first ID read.
    PORTE &= ~_BV(2); (void)polled(0xab); PORTE |= _BV(2); _delay_ms(1);
    begin_id();
    uint8_t first = polled(0), second = polled(0), third = polled(0);
    uint8_t after = SPSR;
    save(0, first, second, third, after, after);
    write_gap<16>(1); write_gap<17>(2); write_gap<18>(3); write_gap<19>(4);
    read_gap<16>(5); read_gap<17>(6); read_gap<18>(7);
    stream<17>(8); stream<18>(9);
    for(unsigned i = 10; i < 12; ++i) save(i, 0xff, 0xff, 0xff, 0xff, 0xff);
    spi_probe_done = 1;
    (void)SPSR; (void)SPDR;
    PORTD &= ~_BV(7); _delay_ms(5); PORTD |= _BV(7); _delay_ms(5);
    uint8_t const commands[] = {
        0xae, 0xd5, 0xf0, 0xa8, 0x3f, 0xd3, 0, 0x40, 0x8d, 0x14,
        0x20, 2, 0xa1, 0xc8, 0xda, 0x12, 0x81, 0x7f, 0xd9, 0xf1,
        0xdb, 0x40, 0xa4, 0xa6, 0xaf,
    };
    PORTD &= ~(_BV(6) | _BV(4));
    for(uint8_t value : commands) send(value);
    PORTD |= _BV(6);
    uint8_t page = 0;
    show_receive(page);
    for(;;)
    {
        if(!(PINE & _BV(6)))
        {
            page ^= 1; show_receive(page);
            while(!(PINE & _BV(6))) {} _delay_ms(100);
        }
        if(!(PINB & _BV(4)))
        {
            page ^= 1; show_receive(page);
            while(!(PINB & _BV(4))) {} _delay_ms(100);
        }
    }
}
