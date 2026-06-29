#pragma once

#include <array>
#include <stdint.h>

namespace absim
{
namespace reg
{

static constexpr uint16_t IO_DATA_OFFSET = 0x20;

constexpr uint16_t data_from_io(uint8_t io_addr)
{
    return uint16_t(io_addr) + IO_DATA_OFFSET;
}

constexpr uint8_t io_from_data(uint8_t data_addr)
{
    return uint8_t(data_addr - IO_DATA_OFFSET);
}

constexpr bool has_io_addr(uint8_t data_addr)
{
    return data_addr >= 0x20 && data_addr <= 0x5f;
}

namespace addr
{
    static constexpr uint8_t PINB   = 0x23;
    static constexpr uint8_t DDRB   = 0x24;
    static constexpr uint8_t PORTB  = 0x25;
    static constexpr uint8_t PINC   = 0x26;
    static constexpr uint8_t DDRC   = 0x27;
    static constexpr uint8_t PORTC  = 0x28;
    static constexpr uint8_t PIND   = 0x29;
    static constexpr uint8_t DDRD   = 0x2a;
    static constexpr uint8_t PORTD  = 0x2b;
    static constexpr uint8_t PINE   = 0x2c;
    static constexpr uint8_t DDRE   = 0x2d;
    static constexpr uint8_t PORTE  = 0x2e;
    static constexpr uint8_t PINF   = 0x2f;
    static constexpr uint8_t DDRF   = 0x30;
    static constexpr uint8_t PORTF  = 0x31;

    static constexpr uint8_t TIFR0  = 0x35;
    static constexpr uint8_t TIFR1  = 0x36;
    static constexpr uint8_t TIFR3  = 0x38;
    static constexpr uint8_t TIFR4  = 0x39;
    static constexpr uint8_t PCIFR  = 0x3b;
    static constexpr uint8_t EIFR   = 0x3c;
    static constexpr uint8_t EIMSK  = 0x3d;
    static constexpr uint8_t GPIOR0 = 0x3e;
    static constexpr uint8_t EECR   = 0x3f;
    static constexpr uint8_t EEDR   = 0x40;
    static constexpr uint8_t EEARL  = 0x41;
    static constexpr uint8_t EEARH  = 0x42;
    static constexpr uint8_t GTCCR  = 0x43;
    static constexpr uint8_t TCCR0A = 0x44;
    static constexpr uint8_t TCCR0B = 0x45;
    static constexpr uint8_t TCNT0  = 0x46;
    static constexpr uint8_t OCR0A  = 0x47;
    static constexpr uint8_t OCR0B  = 0x48;
    static constexpr uint8_t PLLCSR = 0x49;
    static constexpr uint8_t GPIOR1 = 0x4a;
    static constexpr uint8_t GPIOR2 = 0x4b;
    static constexpr uint8_t SPCR   = 0x4c;
    static constexpr uint8_t SPSR   = 0x4d;
    static constexpr uint8_t SPDR   = 0x4e;
    static constexpr uint8_t ACSR   = 0x50;
    static constexpr uint8_t OCDR   = 0x51;
    static constexpr uint8_t MONDR  = OCDR;
    static constexpr uint8_t PLLFRQ = 0x52;
    static constexpr uint8_t SMCR   = 0x53;
    static constexpr uint8_t MCUSR  = 0x54;
    static constexpr uint8_t MCUCR  = 0x55;
    static constexpr uint8_t SPMCSR = 0x57;
    static constexpr uint8_t RAMPZ  = 0x5b;
    static constexpr uint8_t SPL    = 0x5d;
    static constexpr uint8_t SPH    = 0x5e;
    static constexpr uint8_t SREG   = 0x5f;

    static constexpr uint8_t WDTCSR = 0x60;
    static constexpr uint8_t CLKPR  = 0x61;
    static constexpr uint8_t PRR0   = 0x64;
    static constexpr uint8_t PRR1   = 0x65;
    static constexpr uint8_t OSCCAL = 0x66;
    static constexpr uint8_t RCCTRL = 0x67;
    static constexpr uint8_t PCICR  = 0x68;
    static constexpr uint8_t EICRA  = 0x69;
    static constexpr uint8_t EICRB  = 0x6a;
    static constexpr uint8_t PCMSK0 = 0x6b;
    static constexpr uint8_t TIMSK0 = 0x6e;
    static constexpr uint8_t TIMSK1 = 0x6f;
    static constexpr uint8_t TIMSK3 = 0x71;
    static constexpr uint8_t TIMSK4 = 0x72;

    static constexpr uint8_t ADCL   = 0x78;
    static constexpr uint8_t ADCH   = 0x79;
    static constexpr uint8_t ADCSRA = 0x7a;
    static constexpr uint8_t ADCSRB = 0x7b;
    static constexpr uint8_t ADMUX  = 0x7c;
    static constexpr uint8_t DIDR2  = 0x7d;
    static constexpr uint8_t DIDR0  = 0x7e;
    static constexpr uint8_t DIDR1  = 0x7f;

    static constexpr uint8_t TCCR1A = 0x80;
    static constexpr uint8_t TCCR1B = 0x81;
    static constexpr uint8_t TCCR1C = 0x82;
    static constexpr uint8_t TCNT1L = 0x84;
    static constexpr uint8_t TCNT1H = 0x85;
    static constexpr uint8_t ICR1L  = 0x86;
    static constexpr uint8_t ICR1H  = 0x87;
    static constexpr uint8_t OCR1AL = 0x88;
    static constexpr uint8_t OCR1AH = 0x89;
    static constexpr uint8_t OCR1BL = 0x8a;
    static constexpr uint8_t OCR1BH = 0x8b;
    static constexpr uint8_t OCR1CL = 0x8c;
    static constexpr uint8_t OCR1CH = 0x8d;

    static constexpr uint8_t TCCR3A = 0x90;
    static constexpr uint8_t TCCR3B = 0x91;
    static constexpr uint8_t TCCR3C = 0x92;
    static constexpr uint8_t TCNT3L = 0x94;
    static constexpr uint8_t TCNT3H = 0x95;
    static constexpr uint8_t ICR3L  = 0x96;
    static constexpr uint8_t ICR3H  = 0x97;
    static constexpr uint8_t OCR3AL = 0x98;
    static constexpr uint8_t OCR3AH = 0x99;
    static constexpr uint8_t OCR3BL = 0x9a;
    static constexpr uint8_t OCR3BH = 0x9b;
    static constexpr uint8_t OCR3CL = 0x9c;
    static constexpr uint8_t OCR3CH = 0x9d;

    static constexpr uint8_t TWBR  = 0xb8;
    static constexpr uint8_t TWSR  = 0xb9;
    static constexpr uint8_t TWAR  = 0xba;
    static constexpr uint8_t TWDR  = 0xbb;
    static constexpr uint8_t TWCR  = 0xbc;
    static constexpr uint8_t TWAMR = 0xbd;

    static constexpr uint8_t TCNT4 = 0xbe;
    static constexpr uint8_t TC4H  = 0xbf;
    static constexpr uint8_t TCCR4A = 0xc0;
    static constexpr uint8_t TCCR4B = 0xc1;
    static constexpr uint8_t TCCR4C = 0xc2;
    static constexpr uint8_t TCCR4D = 0xc3;
    static constexpr uint8_t TCCR4E = 0xc4;
    static constexpr uint8_t CLKSEL0 = 0xc5;
    static constexpr uint8_t CLKSEL1 = 0xc6;
    static constexpr uint8_t CLKSTA  = 0xc7;
    static constexpr uint8_t UCSR1A = 0xc8;
    static constexpr uint8_t UCSR1B = 0xc9;
    static constexpr uint8_t UCSR1C = 0xca;
    static constexpr uint8_t UCSR1D = 0xcb;
    static constexpr uint8_t UBRR1L = 0xcc;
    static constexpr uint8_t UBRR1H = 0xcd;
    static constexpr uint8_t UDR1   = 0xce;
    static constexpr uint8_t OCR4A  = 0xcf;
    static constexpr uint8_t OCR4B  = 0xd0;
    static constexpr uint8_t OCR4C  = 0xd1;
    static constexpr uint8_t OCR4D  = 0xd2;
    static constexpr uint8_t DT4    = 0xd4;
    static constexpr uint8_t UHWCON = 0xd7;
    static constexpr uint8_t USBCON = 0xd8;
    static constexpr uint8_t USBSTA = 0xd9;
    static constexpr uint8_t USBINT = 0xda;
    static constexpr uint8_t UDCON  = 0xe0;
    static constexpr uint8_t UDINT  = 0xe1;
    static constexpr uint8_t UDIEN  = 0xe2;
    static constexpr uint8_t UDADDR = 0xe3;
    static constexpr uint8_t UDFNUML = 0xe4;
    static constexpr uint8_t UDFNUMH = 0xe5;
    static constexpr uint8_t UDMFN   = 0xe6;
    static constexpr uint8_t UEINTX  = 0xe8;
    static constexpr uint8_t UENUM   = 0xe9;
    static constexpr uint8_t UERST   = 0xea;
    static constexpr uint8_t UECONX  = 0xeb;
    static constexpr uint8_t UECFG0X = 0xec;
    static constexpr uint8_t UECFG1X = 0xed;
    static constexpr uint8_t UESTA0X = 0xee;
    static constexpr uint8_t UESTA1X = 0xef;
    static constexpr uint8_t UEIENX  = 0xf0;
    static constexpr uint8_t UEDATX  = 0xf1;
    static constexpr uint8_t UEBCLX  = 0xf2;
    static constexpr uint8_t UEBCHX  = 0xf3;
    static constexpr uint8_t UEINT   = 0xf4;
}

namespace bit
{
    namespace SREG
    {
        static constexpr uint8_t C = 1u << 0;
        static constexpr uint8_t Z = 1u << 1;
        static constexpr uint8_t N = 1u << 2;
        static constexpr uint8_t V = 1u << 3;
        static constexpr uint8_t S = 1u << 4;
        static constexpr uint8_t H = 1u << 5;
        static constexpr uint8_t T = 1u << 6;
        static constexpr uint8_t I = 1u << 7;
        static constexpr uint8_t HSVNZC = 0x3f;
    }

    namespace PLLCSR
    {
        static constexpr uint8_t PLOCK = 1u << 0;
        static constexpr uint8_t PLLE = 1u << 1;
        static constexpr uint8_t PINDIV = 1u << 4;
    }

    namespace PLLFRQ
    {
        static constexpr uint8_t PDIV0 = 1u << 0;
        static constexpr uint8_t PDIV1 = 1u << 1;
        static constexpr uint8_t PDIV2 = 1u << 2;
        static constexpr uint8_t PDIV3 = 1u << 3;
        static constexpr uint8_t PLLTM0 = 1u << 4;
        static constexpr uint8_t PLLTM1 = 1u << 5;
        static constexpr uint8_t PLLUSB = 1u << 6;
        static constexpr uint8_t PINMUX = 1u << 7;
    }

    namespace GTCCR
    {
        static constexpr uint8_t PSRSYNC = 1u << 0;
        static constexpr uint8_t PSRASY = 1u << 1;
        static constexpr uint8_t TSM = 1u << 7;
    }

    namespace TIFR0
    {
        static constexpr uint8_t TOV0 = 1u << 0;
        static constexpr uint8_t OCF0A = 1u << 1;
        static constexpr uint8_t OCF0B = 1u << 2;
    }

    namespace TIFR1
    {
        static constexpr uint8_t TOV1 = 1u << 0;
        static constexpr uint8_t OCF1A = 1u << 1;
        static constexpr uint8_t OCF1B = 1u << 2;
        static constexpr uint8_t OCF1C = 1u << 3;
        static constexpr uint8_t ICF1 = 1u << 5;
    }

    namespace TIFR3
    {
        static constexpr uint8_t TOV3 = 1u << 0;
        static constexpr uint8_t OCF3A = 1u << 1;
        static constexpr uint8_t OCF3B = 1u << 2;
        static constexpr uint8_t OCF3C = 1u << 3;
        static constexpr uint8_t ICF3 = 1u << 5;
    }

    namespace TIFR4
    {
        static constexpr uint8_t OCF4D = 1u << 7;
        static constexpr uint8_t OCF4A = 1u << 6;
        static constexpr uint8_t OCF4B = 1u << 5;
        static constexpr uint8_t TOV4  = 1u << 2;
    }

    namespace PCIFR
    {
        static constexpr uint8_t PCIF0 = 1u << 0;
    }

    namespace EIFR
    {
        static constexpr uint8_t INTF0 = 1u << 0;
        static constexpr uint8_t INTF1 = 1u << 1;
        static constexpr uint8_t INTF2 = 1u << 2;
        static constexpr uint8_t INTF3 = 1u << 3;
        static constexpr uint8_t INTF6 = 1u << 6;
    }

    namespace EIMSK
    {
        static constexpr uint8_t INT0 = 1u << 0;
        static constexpr uint8_t INT1 = 1u << 1;
        static constexpr uint8_t INT2 = 1u << 2;
        static constexpr uint8_t INT3 = 1u << 3;
        static constexpr uint8_t INT6 = 1u << 6;
    }

    namespace EECR
    {
        static constexpr uint8_t EERE = 1u << 0;
        static constexpr uint8_t EEPE = 1u << 1;
        static constexpr uint8_t EEMPE = 1u << 2;
        static constexpr uint8_t EERIE = 1u << 3;
        static constexpr uint8_t EEPM0 = 1u << 4;
        static constexpr uint8_t EEPM1 = 1u << 5;
    }

    namespace TCCR0A
    {
        static constexpr uint8_t WGM00 = 1u << 0;
        static constexpr uint8_t WGM01 = 1u << 1;
        static constexpr uint8_t COM0B0 = 1u << 4;
        static constexpr uint8_t COM0B1 = 1u << 5;
        static constexpr uint8_t COM0A0 = 1u << 6;
        static constexpr uint8_t COM0A1 = 1u << 7;
    }

    namespace TCCR0B
    {
        static constexpr uint8_t CS00 = 1u << 0;
        static constexpr uint8_t CS01 = 1u << 1;
        static constexpr uint8_t CS02 = 1u << 2;
        static constexpr uint8_t WGM02 = 1u << 3;
        static constexpr uint8_t FOC0B = 1u << 6;
        static constexpr uint8_t FOC0A = 1u << 7;
    }

    namespace SPCR
    {
        static constexpr uint8_t SPR0 = 1u << 0;
        static constexpr uint8_t SPR1 = 1u << 1;
        static constexpr uint8_t CPHA = 1u << 2;
        static constexpr uint8_t CPOL = 1u << 3;
        static constexpr uint8_t MSTR = 1u << 4;
        static constexpr uint8_t DORD = 1u << 5;
        static constexpr uint8_t SPE = 1u << 6;
        static constexpr uint8_t SPIE = 1u << 7;
    }

    namespace SPSR
    {
        static constexpr uint8_t SPI2X = 1u << 0;
        static constexpr uint8_t WCOL = 1u << 6;
        static constexpr uint8_t SPIF = 1u << 7;
    }

    namespace ACSR
    {
        static constexpr uint8_t ACIS0 = 1u << 0;
        static constexpr uint8_t ACIS1 = 1u << 1;
        static constexpr uint8_t ACIC = 1u << 2;
        static constexpr uint8_t ACIE = 1u << 3;
        static constexpr uint8_t ACI = 1u << 4;
        static constexpr uint8_t ACO = 1u << 5;
        static constexpr uint8_t ACBG = 1u << 6;
        static constexpr uint8_t ACD = 1u << 7;
    }

    namespace PLLFRQ_UNUSED
    {
    }

    namespace SMCR
    {
        static constexpr uint8_t SE = 1u << 0;
        static constexpr uint8_t SM0 = 1u << 1;
        static constexpr uint8_t SM1 = 1u << 2;
        static constexpr uint8_t SM2 = 1u << 3;
    }

    namespace MCUSR
    {
        static constexpr uint8_t PORF = 1u << 0;
        static constexpr uint8_t EXTRF = 1u << 1;
        static constexpr uint8_t BORF = 1u << 2;
        static constexpr uint8_t WDRF = 1u << 3;
        static constexpr uint8_t JTRF = 1u << 4;
        static constexpr uint8_t USBRF = 1u << 5;
    }

    namespace MCUCR
    {
        static constexpr uint8_t IVCE = 1u << 0;
        static constexpr uint8_t IVSEL = 1u << 1;
        static constexpr uint8_t PUD = 1u << 4;
        static constexpr uint8_t JTD = 1u << 7;
    }

    namespace SPMCSR
    {
        static constexpr uint8_t SPMEN = 1u << 0;
        static constexpr uint8_t PGERS = 1u << 1;
        static constexpr uint8_t PGWRT = 1u << 2;
        static constexpr uint8_t BLBSET = 1u << 3;
        static constexpr uint8_t RWWSRE = 1u << 4;
        static constexpr uint8_t SIGRD = 1u << 5;
        static constexpr uint8_t RWWSB = 1u << 6;
        static constexpr uint8_t SPMIE = 1u << 7;
    }

    namespace WDTCSR
    {
        static constexpr uint8_t WDP0 = 1u << 0;
        static constexpr uint8_t WDP1 = 1u << 1;
        static constexpr uint8_t WDP2 = 1u << 2;
        static constexpr uint8_t WDE = 1u << 3;
        static constexpr uint8_t WDCE = 1u << 4;
        static constexpr uint8_t WDP3 = 1u << 5;
        static constexpr uint8_t WDIE = 1u << 6;
        static constexpr uint8_t WDIF = 1u << 7;
    }

    namespace CLKPR
    {
        static constexpr uint8_t CLKPS0 = 1u << 0;
        static constexpr uint8_t CLKPS1 = 1u << 1;
        static constexpr uint8_t CLKPS2 = 1u << 2;
        static constexpr uint8_t CLKPS3 = 1u << 3;
        static constexpr uint8_t CLKPCE = 1u << 7;
    }

    namespace PRR0
    {
        static constexpr uint8_t PRADC = 1u << 0;
        static constexpr uint8_t PRSPI = 1u << 2;
        static constexpr uint8_t PRTIM1 = 1u << 3;
        static constexpr uint8_t PRTIM0 = 1u << 5;
        static constexpr uint8_t PRTWI = 1u << 7;
    }

    namespace PRR1
    {
        static constexpr uint8_t PRUSART1 = 1u << 0;
        static constexpr uint8_t PRTIM4 = 1u << 4;
        static constexpr uint8_t PRTIM3 = 1u << 3;
        static constexpr uint8_t PRUSB = 1u << 7;
    }

    namespace ADCSRA
    {
        static constexpr uint8_t ADPS0 = 1u << 0;
        static constexpr uint8_t ADPS1 = 1u << 1;
        static constexpr uint8_t ADPS2 = 1u << 2;
        static constexpr uint8_t ADIE = 1u << 3;
        static constexpr uint8_t ADIF = 1u << 4;
        static constexpr uint8_t ADATE = 1u << 5;
        static constexpr uint8_t ADSC = 1u << 6;
        static constexpr uint8_t ADEN = 1u << 7;
    }

    namespace ADCSRB
    {
        static constexpr uint8_t ADTS0 = 1u << 0;
        static constexpr uint8_t ADTS1 = 1u << 1;
        static constexpr uint8_t ADTS2 = 1u << 2;
        static constexpr uint8_t ADTS3 = 1u << 3;
        static constexpr uint8_t MUX5 = 1u << 5;
        static constexpr uint8_t ACME = 1u << 6;
        static constexpr uint8_t ADHSM = 1u << 7;
    }

    namespace ADMUX
    {
        static constexpr uint8_t MUX0 = 1u << 0;
        static constexpr uint8_t MUX1 = 1u << 1;
        static constexpr uint8_t MUX2 = 1u << 2;
        static constexpr uint8_t MUX3 = 1u << 3;
        static constexpr uint8_t MUX4 = 1u << 4;
        static constexpr uint8_t ADLAR = 1u << 5;
        static constexpr uint8_t REFS0 = 1u << 6;
        static constexpr uint8_t REFS1 = 1u << 7;
    }

    namespace DIDR0
    {
        static constexpr uint8_t ADC0D = 1u << 0;
        static constexpr uint8_t ADC1D = 1u << 1;
        static constexpr uint8_t ADC2D = 1u << 2;
        static constexpr uint8_t ADC3D = 1u << 3;
        static constexpr uint8_t ADC4D = 1u << 4;
        static constexpr uint8_t ADC5D = 1u << 5;
        static constexpr uint8_t ADC6D = 1u << 6;
        static constexpr uint8_t ADC7D = 1u << 7;
    }

    namespace DIDR1
    {
        static constexpr uint8_t AIN0D = 1u << 0;
    }

    namespace DIDR2
    {
        static constexpr uint8_t ADC8D = 1u << 0;
        static constexpr uint8_t ADC9D = 1u << 1;
        static constexpr uint8_t ADC10D = 1u << 2;
        static constexpr uint8_t ADC11D = 1u << 3;
        static constexpr uint8_t ADC12D = 1u << 4;
        static constexpr uint8_t ADC13D = 1u << 5;
    }

    namespace TCCR1A
    {
        static constexpr uint8_t WGM10 = 1u << 0;
        static constexpr uint8_t WGM11 = 1u << 1;
        static constexpr uint8_t COM1C0 = 1u << 2;
        static constexpr uint8_t COM1C1 = 1u << 3;
        static constexpr uint8_t COM1B0 = 1u << 4;
        static constexpr uint8_t COM1B1 = 1u << 5;
        static constexpr uint8_t COM1A0 = 1u << 6;
        static constexpr uint8_t COM1A1 = 1u << 7;
    }

    namespace TCCR1B
    {
        static constexpr uint8_t CS10 = 1u << 0;
        static constexpr uint8_t CS11 = 1u << 1;
        static constexpr uint8_t CS12 = 1u << 2;
        static constexpr uint8_t WGM12 = 1u << 3;
        static constexpr uint8_t WGM13 = 1u << 4;
        static constexpr uint8_t ICES1 = 1u << 6;
        static constexpr uint8_t ICNC1 = 1u << 7;
    }

    namespace TCCR1C
    {
        static constexpr uint8_t FOC1A = 1u << 7;
        static constexpr uint8_t FOC1B = 1u << 6;
        static constexpr uint8_t FOC1C = 1u << 5;
    }

    namespace TCCR3A
    {
        static constexpr uint8_t WGM30 = 1u << 0;
        static constexpr uint8_t WGM31 = 1u << 1;
        static constexpr uint8_t COM3C0 = 1u << 2;
        static constexpr uint8_t COM3C1 = 1u << 3;
        static constexpr uint8_t COM3B0 = 1u << 4;
        static constexpr uint8_t COM3B1 = 1u << 5;
        static constexpr uint8_t COM3A0 = 1u << 6;
        static constexpr uint8_t COM3A1 = 1u << 7;
    }

    namespace TCCR3B
    {
        static constexpr uint8_t CS30 = 1u << 0;
        static constexpr uint8_t CS31 = 1u << 1;
        static constexpr uint8_t CS32 = 1u << 2;
        static constexpr uint8_t WGM32 = 1u << 3;
        static constexpr uint8_t WGM33 = 1u << 4;
        static constexpr uint8_t ICES3 = 1u << 6;
        static constexpr uint8_t ICNC3 = 1u << 7;
    }

    namespace TCCR3C
    {
        static constexpr uint8_t FOC3A = 1u << 7;
        static constexpr uint8_t FOC3B = 1u << 6;
        static constexpr uint8_t FOC3C = 1u << 5;
    }

    namespace TC4H
    {
        static constexpr uint8_t TC4H0 = 1u << 0;
        static constexpr uint8_t TC4H1 = 1u << 1;
        static constexpr uint8_t TC4H2 = 1u << 2;
    }

    namespace TCCR4A
    {
        static constexpr uint8_t PWM4B = 1u << 0;
        static constexpr uint8_t PWM4A = 1u << 1;
        static constexpr uint8_t FOC4B = 1u << 2;
        static constexpr uint8_t FOC4A = 1u << 3;
        static constexpr uint8_t COM4B0 = 1u << 4;
        static constexpr uint8_t COM4B1 = 1u << 5;
        static constexpr uint8_t COM4A0 = 1u << 6;
        static constexpr uint8_t COM4A1 = 1u << 7;
    }

    namespace TCCR4B
    {
        static constexpr uint8_t CS40 = 1u << 0;
        static constexpr uint8_t CS41 = 1u << 1;
        static constexpr uint8_t CS42 = 1u << 2;
        static constexpr uint8_t CS43 = 1u << 3;
        static constexpr uint8_t DTPS40 = 1u << 4;
        static constexpr uint8_t DTPS41 = 1u << 5;
        static constexpr uint8_t PSR4 = 1u << 6;
        static constexpr uint8_t PWM4X = 1u << 7;
    }

    namespace TCCR4C
    {
        static constexpr uint8_t PWM4D = 1u << 0;
        static constexpr uint8_t FOC4D = 1u << 1;
        static constexpr uint8_t COM4D0S = 1u << 2;
        static constexpr uint8_t COM4D1S = 1u << 3;
        static constexpr uint8_t COM4B0S = 1u << 4;
        static constexpr uint8_t COM4B1S = 1u << 5;
        static constexpr uint8_t COM4A0S = 1u << 6;
        static constexpr uint8_t COM4A1S = 1u << 7;
    }

    namespace TCCR4D
    {
        static constexpr uint8_t WGM40 = 1u << 0;
        static constexpr uint8_t WGM41 = 1u << 1;
        static constexpr uint8_t FPF4 = 1u << 2;
        static constexpr uint8_t FPAC4 = 1u << 3;
        static constexpr uint8_t FPES4 = 1u << 4;
        static constexpr uint8_t FPNC4 = 1u << 5;
        static constexpr uint8_t FPEN4 = 1u << 6;
        static constexpr uint8_t FPIE4 = 1u << 7;
    }

    namespace TCCR4E
    {
        static constexpr uint8_t OC4OE0 = 1u << 0;
        static constexpr uint8_t OC4OE1 = 1u << 1;
        static constexpr uint8_t OC4OE2 = 1u << 2;
        static constexpr uint8_t OC4OE3 = 1u << 3;
        static constexpr uint8_t OC4OE4 = 1u << 4;
        static constexpr uint8_t OC4OE5 = 1u << 5;
        static constexpr uint8_t ENHC4 = 1u << 6;
        static constexpr uint8_t TLOCK4 = 1u << 7;
    }

    namespace TIMSK0
    {
        static constexpr uint8_t TOIE0 = 1u << 0;
        static constexpr uint8_t OCIE0A = 1u << 1;
        static constexpr uint8_t OCIE0B = 1u << 2;
    }

    namespace TIMSK1
    {
        static constexpr uint8_t TOIE1 = 1u << 0;
        static constexpr uint8_t OCIE1A = 1u << 1;
        static constexpr uint8_t OCIE1B = 1u << 2;
        static constexpr uint8_t OCIE1C = 1u << 3;
        static constexpr uint8_t ICIE1 = 1u << 5;
    }

    namespace TIMSK3
    {
        static constexpr uint8_t TOIE3 = 1u << 0;
        static constexpr uint8_t OCIE3A = 1u << 1;
        static constexpr uint8_t OCIE3B = 1u << 2;
        static constexpr uint8_t OCIE3C = 1u << 3;
        static constexpr uint8_t ICIE3 = 1u << 5;
    }

    namespace TIMSK4
    {
        static constexpr uint8_t TOIE4 = 1u << 2;
        static constexpr uint8_t OCIE4B = 1u << 5;
        static constexpr uint8_t OCIE4A = 1u << 6;
        static constexpr uint8_t OCIE4D = 1u << 7;
    }

    namespace TWCR
    {
        static constexpr uint8_t TWIE = 1u << 0;
        static constexpr uint8_t TWEA = 1u << 1;
        static constexpr uint8_t TWSTA = 1u << 2;
        static constexpr uint8_t TWSTO = 1u << 3;
        static constexpr uint8_t TWWC = 1u << 4;
        static constexpr uint8_t TWEN = 1u << 5;
        static constexpr uint8_t TWINT = 1u << 7;
    }

    namespace TWAR
    {
        static constexpr uint8_t TWGCE = 1u << 0;
    }

    namespace TWSR
    {
        static constexpr uint8_t TWPS0 = 1u << 0;
        static constexpr uint8_t TWPS1 = 1u << 1;
    }

    namespace CLKSEL0
    {
        static constexpr uint8_t CLKS = 1u << 0;
        static constexpr uint8_t EXTE = 1u << 2;
        static constexpr uint8_t RCE = 1u << 3;
        static constexpr uint8_t EXSUT0 = 1u << 4;
        static constexpr uint8_t EXSUT1 = 1u << 5;
        static constexpr uint8_t RCSUT0 = 1u << 6;
        static constexpr uint8_t RCSUT1 = 1u << 7;
    }

    namespace CLKSEL1
    {
        static constexpr uint8_t EXCKSEL0 = 1u << 0;
        static constexpr uint8_t EXCKSEL1 = 1u << 1;
        static constexpr uint8_t EXCKSEL2 = 1u << 2;
        static constexpr uint8_t EXCKSEL3 = 1u << 3;
        static constexpr uint8_t RCCKSEL0 = 1u << 4;
        static constexpr uint8_t RCCKSEL1 = 1u << 5;
        static constexpr uint8_t RCCKSEL2 = 1u << 6;
        static constexpr uint8_t RCCKSEL3 = 1u << 7;
    }

    namespace CLKSTA
    {
        static constexpr uint8_t EXTON = 1u << 0;
        static constexpr uint8_t RCON = 1u << 1;
    }

    namespace UCSR1A
    {
        static constexpr uint8_t MPCM1 = 1u << 0;
        static constexpr uint8_t U2X1 = 1u << 1;
        static constexpr uint8_t UPE1 = 1u << 2;
        static constexpr uint8_t DOR1 = 1u << 3;
        static constexpr uint8_t FE1 = 1u << 4;
        static constexpr uint8_t UDRE1 = 1u << 5;
        static constexpr uint8_t TXC1 = 1u << 6;
        static constexpr uint8_t RXC1 = 1u << 7;
    }

    namespace UCSR1B
    {
        static constexpr uint8_t TXB81 = 1u << 0;
        static constexpr uint8_t RXB81 = 1u << 1;
        static constexpr uint8_t UCSZ12 = 1u << 2;
        static constexpr uint8_t TXEN1 = 1u << 3;
        static constexpr uint8_t RXEN1 = 1u << 4;
        static constexpr uint8_t UDRIE1 = 1u << 5;
        static constexpr uint8_t TXCIE1 = 1u << 6;
        static constexpr uint8_t RXCIE1 = 1u << 7;
    }

    namespace UCSR1C
    {
        static constexpr uint8_t UCPOL1 = 1u << 0;
        static constexpr uint8_t UCSZ10 = 1u << 1;
        static constexpr uint8_t UCSZ11 = 1u << 2;
        static constexpr uint8_t USBS1 = 1u << 3;
        static constexpr uint8_t UPM10 = 1u << 4;
        static constexpr uint8_t UPM11 = 1u << 5;
        static constexpr uint8_t UMSEL10 = 1u << 6;
        static constexpr uint8_t UMSEL11 = 1u << 7;
    }

    namespace UCSR1D
    {
        static constexpr uint8_t RTSEN = 1u << 0;
        static constexpr uint8_t CTSEN = 1u << 1;
    }

    namespace UHWCON
    {
        static constexpr uint8_t UVREGE = 1u << 0;
    }

    namespace USBCON
    {
        static constexpr uint8_t USBE = 1u << 7;
        static constexpr uint8_t FRZCLK = 1u << 5;
        static constexpr uint8_t OTGPADE = 1u << 4;
        static constexpr uint8_t VBUSTE = 1u << 0;
    }

    namespace USBSTA
    {
        static constexpr uint8_t VBUS = 1u << 0;
        static constexpr uint8_t ID = 1u << 1;
    }

    namespace USBINT
    {
        static constexpr uint8_t VBUSTI = 1u << 0;
    }

    namespace UDCON
    {
        static constexpr uint8_t DETACH = 1u << 0;
        static constexpr uint8_t RMWKUP = 1u << 1;
        static constexpr uint8_t LSM = 1u << 2;
        static constexpr uint8_t RSTCPU = 1u << 3;
    }

    namespace UDINT
    {
        static constexpr uint8_t SUSPI = 1u << 0;
        static constexpr uint8_t MSOFI = 1u << 1;
        static constexpr uint8_t SOFI = 1u << 2;
        static constexpr uint8_t EORSTI = 1u << 3;
        static constexpr uint8_t WAKEUPI = 1u << 4;
        static constexpr uint8_t EORSMI = 1u << 5;
        static constexpr uint8_t UPRSMI = 1u << 6;
    }

    namespace UDIEN
    {
        static constexpr uint8_t SUSPE = 1u << 0;
        static constexpr uint8_t MSOFE = 1u << 1;
        static constexpr uint8_t SOFE = 1u << 2;
        static constexpr uint8_t EORSTE = 1u << 3;
        static constexpr uint8_t WAKEUPE = 1u << 4;
        static constexpr uint8_t EORSME = 1u << 5;
        static constexpr uint8_t UPRSME = 1u << 6;
    }

    namespace UDADDR
    {
        static constexpr uint8_t ADDEN = 1u << 7;
    }

    namespace UDMFN
    {
        static constexpr uint8_t FNCERR = 1u << 3;
    }

    namespace UEINTX
    {
        static constexpr uint8_t TXINI = 1u << 0;
        static constexpr uint8_t STALLEDI = 1u << 1;
        static constexpr uint8_t RXOUTI = 1u << 2;
        static constexpr uint8_t RXSTPI = 1u << 3;
        static constexpr uint8_t NAKOUTI = 1u << 4;
        static constexpr uint8_t RWAL = 1u << 5;
        static constexpr uint8_t NAKINI = 1u << 6;
        static constexpr uint8_t FIFOCON = 1u << 7;
    }

    namespace UENUM
    {
        static constexpr uint8_t EPNUM0 = 1u << 0;
        static constexpr uint8_t EPNUM1 = 1u << 1;
        static constexpr uint8_t EPNUM2 = 1u << 2;
    }

    namespace UERST
    {
        static constexpr uint8_t EPRST0 = 1u << 0;
        static constexpr uint8_t EPRST1 = 1u << 1;
        static constexpr uint8_t EPRST2 = 1u << 2;
        static constexpr uint8_t EPRST3 = 1u << 3;
        static constexpr uint8_t EPRST4 = 1u << 4;
        static constexpr uint8_t EPRST5 = 1u << 5;
        static constexpr uint8_t EPRST6 = 1u << 6;
    }

    namespace UECONX
    {
        static constexpr uint8_t EPEN = 1u << 0;
        static constexpr uint8_t RSTDT = 1u << 3;
        static constexpr uint8_t STALLRQC = 1u << 4;
        static constexpr uint8_t STALLRQ = 1u << 5;
    }

    namespace UECFG0X
    {
        static constexpr uint8_t EPDIR = 1u << 0;
        static constexpr uint8_t EPTYPE0 = 1u << 6;
        static constexpr uint8_t EPTYPE1 = 1u << 7;
    }

    namespace UECFG1X
    {
        static constexpr uint8_t ALLOC = 1u << 1;
        static constexpr uint8_t EPBK0 = 1u << 2;
        static constexpr uint8_t EPBK1 = 1u << 3;
        static constexpr uint8_t EPSIZE0 = 1u << 4;
        static constexpr uint8_t EPSIZE1 = 1u << 5;
        static constexpr uint8_t EPSIZE2 = 1u << 6;
    }

    namespace UESTA0X
    {
        static constexpr uint8_t CFGOK = 1u << 7;
        static constexpr uint8_t OVERFI = 1u << 6;
        static constexpr uint8_t UNDERFI = 1u << 5;
        static constexpr uint8_t DTSEQ0 = 1u << 2;
        static constexpr uint8_t DTSEQ1 = 1u << 3;
        static constexpr uint8_t NBUSYBK0 = 1u << 0;
        static constexpr uint8_t NBUSYBK1 = 1u << 1;
    }

    namespace UESTA1X
    {
        static constexpr uint8_t CURRBK0 = 1u << 0;
        static constexpr uint8_t CURRBK1 = 1u << 1;
        static constexpr uint8_t CTRLDIR = 1u << 2;
    }

    namespace UEIENX
    {
        static constexpr uint8_t FLERRE = 1u << 7;
        static constexpr uint8_t NAKINE = 1u << 6;
        static constexpr uint8_t NAKOUTE = 1u << 4;
        static constexpr uint8_t RXSTPE = 1u << 3;
        static constexpr uint8_t RXOUTE = 1u << 2;
        static constexpr uint8_t STALLEDE = 1u << 1;
        static constexpr uint8_t TXINE = 1u << 0;
    }

    namespace UEINT
    {
        static constexpr uint8_t EPINT0 = 1u << 0;
        static constexpr uint8_t EPINT1 = 1u << 1;
        static constexpr uint8_t EPINT2 = 1u << 2;
        static constexpr uint8_t EPINT3 = 1u << 3;
        static constexpr uint8_t EPINT4 = 1u << 4;
        static constexpr uint8_t EPINT5 = 1u << 5;
        static constexpr uint8_t EPINT6 = 1u << 6;
    }

    namespace PINB
    {
        static constexpr uint8_t PINB0 = 1u << 0;
        static constexpr uint8_t PINB1 = 1u << 1;
        static constexpr uint8_t PINB2 = 1u << 2;
        static constexpr uint8_t PINB3 = 1u << 3;
        static constexpr uint8_t PINB4 = 1u << 4;
        static constexpr uint8_t PINB5 = 1u << 5;
        static constexpr uint8_t PINB6 = 1u << 6;
        static constexpr uint8_t PINB7 = 1u << 7;
    }

    namespace DDRB
    {
        static constexpr uint8_t DDB0 = 1u << 0;
        static constexpr uint8_t DDB1 = 1u << 1;
        static constexpr uint8_t DDB2 = 1u << 2;
        static constexpr uint8_t DDB3 = 1u << 3;
        static constexpr uint8_t DDB4 = 1u << 4;
        static constexpr uint8_t DDB5 = 1u << 5;
        static constexpr uint8_t DDB6 = 1u << 6;
        static constexpr uint8_t DDB7 = 1u << 7;
    }

    namespace PORTB
    {
        static constexpr uint8_t PORTB0 = 1u << 0;
        static constexpr uint8_t PORTB1 = 1u << 1;
        static constexpr uint8_t PORTB2 = 1u << 2;
        static constexpr uint8_t PORTB3 = 1u << 3;
        static constexpr uint8_t PORTB4 = 1u << 4;
        static constexpr uint8_t PORTB5 = 1u << 5;
        static constexpr uint8_t PORTB6 = 1u << 6;
        static constexpr uint8_t PORTB7 = 1u << 7;
    }

    namespace PINC
    {
        static constexpr uint8_t PINC6 = 1u << 6;
        static constexpr uint8_t PINC7 = 1u << 7;
    }

    namespace DDRC
    {
        static constexpr uint8_t DDC6 = 1u << 6;
        static constexpr uint8_t DDC7 = 1u << 7;
    }

    namespace PORTC
    {
        static constexpr uint8_t PORTC6 = 1u << 6;
        static constexpr uint8_t PORTC7 = 1u << 7;
    }

    namespace PIND
    {
        static constexpr uint8_t PIND0 = 1u << 0;
        static constexpr uint8_t PIND1 = 1u << 1;
        static constexpr uint8_t PIND2 = 1u << 2;
        static constexpr uint8_t PIND3 = 1u << 3;
        static constexpr uint8_t PIND4 = 1u << 4;
        static constexpr uint8_t PIND5 = 1u << 5;
        static constexpr uint8_t PIND6 = 1u << 6;
        static constexpr uint8_t PIND7 = 1u << 7;
    }

    namespace DDRD
    {
        static constexpr uint8_t DDD0 = 1u << 0;
        static constexpr uint8_t DDD1 = 1u << 1;
        static constexpr uint8_t DDD2 = 1u << 2;
        static constexpr uint8_t DDD3 = 1u << 3;
        static constexpr uint8_t DDD4 = 1u << 4;
        static constexpr uint8_t DDD5 = 1u << 5;
        static constexpr uint8_t DDD6 = 1u << 6;
        static constexpr uint8_t DDD7 = 1u << 7;
    }

    namespace PORTD
    {
        static constexpr uint8_t PORTD0 = 1u << 0;
        static constexpr uint8_t PORTD1 = 1u << 1;
        static constexpr uint8_t PORTD2 = 1u << 2;
        static constexpr uint8_t PORTD3 = 1u << 3;
        static constexpr uint8_t PORTD4 = 1u << 4;
        static constexpr uint8_t PORTD5 = 1u << 5;
        static constexpr uint8_t PORTD6 = 1u << 6;
        static constexpr uint8_t PORTD7 = 1u << 7;
    }

    namespace PINE
    {
        static constexpr uint8_t PINE2 = 1u << 2;
        static constexpr uint8_t PINE6 = 1u << 6;
    }

    namespace DDRE
    {
        static constexpr uint8_t DDE2 = 1u << 2;
        static constexpr uint8_t DDE6 = 1u << 6;
    }

    namespace PORTE
    {
        static constexpr uint8_t PORTE2 = 1u << 2;
        static constexpr uint8_t PORTE6 = 1u << 6;
    }

    namespace PINF
    {
        static constexpr uint8_t PINF0 = 1u << 0;
        static constexpr uint8_t PINF1 = 1u << 1;
        static constexpr uint8_t PINF4 = 1u << 4;
        static constexpr uint8_t PINF5 = 1u << 5;
        static constexpr uint8_t PINF6 = 1u << 6;
        static constexpr uint8_t PINF7 = 1u << 7;
    }

    namespace DDRF
    {
        static constexpr uint8_t DDF0 = 1u << 0;
        static constexpr uint8_t DDF1 = 1u << 1;
        static constexpr uint8_t DDF4 = 1u << 4;
        static constexpr uint8_t DDF5 = 1u << 5;
        static constexpr uint8_t DDF6 = 1u << 6;
        static constexpr uint8_t DDF7 = 1u << 7;
    }

    namespace PORTF
    {
        static constexpr uint8_t PORTF0 = 1u << 0;
        static constexpr uint8_t PORTF1 = 1u << 1;
        static constexpr uint8_t PORTF4 = 1u << 4;
        static constexpr uint8_t PORTF5 = 1u << 5;
        static constexpr uint8_t PORTF6 = 1u << 6;
        static constexpr uint8_t PORTF7 = 1u << 7;
    }

    namespace GPIOR0
    {
        static constexpr uint8_t GPIOR00 = 1u << 0;
        static constexpr uint8_t GPIOR01 = 1u << 1;
        static constexpr uint8_t GPIOR02 = 1u << 2;
        static constexpr uint8_t GPIOR03 = 1u << 3;
        static constexpr uint8_t GPIOR04 = 1u << 4;
        static constexpr uint8_t GPIOR05 = 1u << 5;
        static constexpr uint8_t GPIOR06 = 1u << 6;
        static constexpr uint8_t GPIOR07 = 1u << 7;
    }

    namespace GPIOR1
    {
        static constexpr uint8_t GPIOR10 = 1u << 0;
        static constexpr uint8_t GPIOR11 = 1u << 1;
        static constexpr uint8_t GPIOR12 = 1u << 2;
        static constexpr uint8_t GPIOR13 = 1u << 3;
        static constexpr uint8_t GPIOR14 = 1u << 4;
        static constexpr uint8_t GPIOR15 = 1u << 5;
        static constexpr uint8_t GPIOR16 = 1u << 6;
        static constexpr uint8_t GPIOR17 = 1u << 7;
    }

    namespace GPIOR2
    {
        static constexpr uint8_t GPIOR20 = 1u << 0;
        static constexpr uint8_t GPIOR21 = 1u << 1;
        static constexpr uint8_t GPIOR22 = 1u << 2;
        static constexpr uint8_t GPIOR23 = 1u << 3;
        static constexpr uint8_t GPIOR24 = 1u << 4;
        static constexpr uint8_t GPIOR25 = 1u << 5;
        static constexpr uint8_t GPIOR26 = 1u << 6;
        static constexpr uint8_t GPIOR27 = 1u << 7;
    }

    namespace SPL
    {
        static constexpr uint8_t SP0 = 1u << 0;
        static constexpr uint8_t SP1 = 1u << 1;
        static constexpr uint8_t SP2 = 1u << 2;
        static constexpr uint8_t SP3 = 1u << 3;
        static constexpr uint8_t SP4 = 1u << 4;
        static constexpr uint8_t SP5 = 1u << 5;
        static constexpr uint8_t SP6 = 1u << 6;
        static constexpr uint8_t SP7 = 1u << 7;
    }

    namespace SPH
    {
        static constexpr uint8_t SP8 = 1u << 0;
        static constexpr uint8_t SP9 = 1u << 1;
        static constexpr uint8_t SP10 = 1u << 2;
        static constexpr uint8_t SP11 = 1u << 3;
        static constexpr uint8_t SP12 = 1u << 4;
        static constexpr uint8_t SP13 = 1u << 5;
        static constexpr uint8_t SP14 = 1u << 6;
        static constexpr uint8_t SP15 = 1u << 7;
    }
}

struct register_info_t
{
    char const* name;
    uint8_t read_mask;
    uint8_t write_mask;
    uint8_t reset_value;
    std::array<char const*, 8> bits;
};

constexpr std::array<char const*, 8> make_bits(
    char const* b7, char const* b6, char const* b5, char const* b4,
    char const* b3, char const* b2, char const* b1, char const* b0)
{
    return {{ b0, b1, b2, b3, b4, b5, b6, b7 }};
}

static constexpr std::array<register_info_t, 256> REGISTER_INFO = {{
    /* 0x00 */ {},
    /* 0x01 */ {},
    /* 0x02 */ {},
    /* 0x03 */ {},
    /* 0x04 */ {},
    /* 0x05 */ {},
    /* 0x06 */ {},
    /* 0x07 */ {},
    /* 0x08 */ {},
    /* 0x09 */ {},
    /* 0x0a */ {},
    /* 0x0b */ {},
    /* 0x0c */ {},
    /* 0x0d */ {},
    /* 0x0e */ {},
    /* 0x0f */ {},
    /* 0x10 */ {},
    /* 0x11 */ {},
    /* 0x12 */ {},
    /* 0x13 */ {},
    /* 0x14 */ {},
    /* 0x15 */ {},
    /* 0x16 */ {},
    /* 0x17 */ {},
    /* 0x18 */ {},
    /* 0x19 */ {},
    /* 0x1a */ {},
    /* 0x1b */ {},
    /* 0x1c */ {},
    /* 0x1d */ {},
    /* 0x1e */ {},
    /* 0x1f */ {},
    /* 0x20 */ {},
    /* 0x21 */ {},
    /* 0x22 */ {},
    /* 0x23 PINB */ {
        "PINB", 0xff, 0xff, 0x00,
        make_bits(
            "PINB7", "PINB6", "PINB5", "PINB4",
            "PINB3", "PINB2", "PINB1", "PINB0"),
    },
    /* 0x24 DDRB */ {
        "DDRB", 0xff, 0xff, 0x00,
        make_bits(
            "DDB7", "DDB6", "DDB5", "DDB4",
            "DDB3", "DDB2", "DDB1", "DDB0"),
    },
    /* 0x25 PORTB */ {
        "PORTB", 0xff, 0xff, 0x00,
        make_bits(
            "PORTB7", "PORTB6", "PORTB5", "PORTB4",
            "PORTB3", "PORTB2", "PORTB1", "PORTB0"),
    },
    /* 0x26 PINC */ {
        "PINC", 0xc0, 0xc0, 0x00,
        make_bits(
            "PINC7", "PINC6", nullptr, nullptr,
            nullptr, nullptr, nullptr, nullptr),
    },
    /* 0x27 DDRC */ {
        "DDRC", 0xc0, 0xc0, 0x00,
        make_bits(
            "DDC7", "DDC6", nullptr, nullptr,
            nullptr, nullptr, nullptr, nullptr),
    },
    /* 0x28 PORTC */ {
        "PORTC", 0xc0, 0xc0, 0x00,
        make_bits(
            "PORTC7", "PORTC6", nullptr, nullptr,
            nullptr, nullptr, nullptr, nullptr),
    },
    /* 0x29 PIND */ {
        "PIND", 0xff, 0xff, 0x00,
        make_bits(
            "PIND7", "PIND6", "PIND5", "PIND4",
            "PIND3", "PIND2", "PIND1", "PIND0"),
    },
    /* 0x2a DDRD */ {
        "DDRD", 0xff, 0xff, 0x00,
        make_bits(
            "DDD7", "DDD6", "DDD5", "DDD4",
            "DDD3", "DDD2", "DDD1", "DDD0"),
    },
    /* 0x2b PORTD */ {
        "PORTD", 0xff, 0xff, 0x00,
        make_bits(
            "PORTD7", "PORTD6", "PORTD5", "PORTD4",
            "PORTD3", "PORTD2", "PORTD1", "PORTD0"),
    },
    /* 0x2c PINE */ {
        "PINE", 0x44, 0x44, 0x00,
        make_bits(
            nullptr, "PINE6", nullptr, nullptr,
            nullptr, "PINE2", nullptr, nullptr),
    },
    /* 0x2d DDRE */ {
        "DDRE", 0x44, 0x44, 0x00,
        make_bits(
            nullptr, "DDE6", nullptr, nullptr,
            nullptr, "DDE2", nullptr, nullptr),
    },
    /* 0x2e PORTE */ {
        "PORTE", 0x44, 0x44, 0x00,
        make_bits(
            nullptr, "PORTE6", nullptr, nullptr,
            nullptr, "PORTE2", nullptr, nullptr),
    },
    /* 0x2f PINF */ {
        "PINF", 0xf3, 0xf3, 0x00,
        make_bits(
            "PINF7", "PINF6", "PINF5", "PINF4",
            nullptr, nullptr, "PINF1", "PINF0"),
    },
    /* 0x30 DDRF */ {
        "DDRF", 0xf3, 0xf3, 0x00,
        make_bits(
            "DDF7", "DDF6", "DDF5", "DDF4",
            nullptr, nullptr, "DDF1", "DDF0"),
    },
    /* 0x31 PORTF */ {
        "PORTF", 0xf3, 0xf3, 0x00,
        make_bits(
            "PORTF7", "PORTF6", "PORTF5", "PORTF4",
            nullptr, nullptr, "PORTF1", "PORTF0"),
    },
    /* 0x32 */ {},
    /* 0x33 */ {},
    /* 0x34 */ {},
    /* 0x35 TIFR0 */ {
        "TIFR0", 0x07, 0x07, 0x00,
        make_bits(
            nullptr, nullptr, nullptr, nullptr,
            nullptr, "OCF0B", "OCF0A", "TOV0"),
    },
    /* 0x36 TIFR1 */ {
        "TIFR1", 0x2f, 0x2f, 0x00,
        make_bits(
            nullptr, nullptr, "ICF1", nullptr,
            "OCF1C", "OCF1B", "OCF1A", "TOV1"),
    },
    /* 0x37 */ {},
    /* 0x38 TIFR3 */ {
        "TIFR3", 0x2f, 0x2f, 0x00,
        make_bits(
            nullptr, nullptr, "ICF3", nullptr,
            "OCF3C", "OCF3B", "OCF3A", "TOV3"),
    },
    /* 0x39 TIFR4 */ {
        "TIFR4", 0xe4, 0xe4, 0x00,
        make_bits(
            "OCF4D", "OCF4A", "OCF4B", nullptr,
            nullptr, "TOV4", nullptr, nullptr),
    },
    /* 0x3a */ {},
    /* 0x3b PCIFR */ {
        "PCIFR", 0x01, 0x01, 0x00,
        make_bits(
            nullptr, nullptr, nullptr, nullptr,
            nullptr, nullptr, nullptr, "PCIF0"),
    },
    /* 0x3c EIFR */ {
        "EIFR", 0x4f, 0x4f, 0x00,
        make_bits(
            nullptr, "INTF6", nullptr, nullptr,
            "INTF3", "INTF2", "INTF1", "INTF0"),
    },
    /* 0x3d EIMSK */ {
        "EIMSK", 0x4f, 0x4f, 0x00,
        make_bits(
            nullptr, "INT6", nullptr, nullptr,
            "INT3", "INT2", "INT1", "INT0"),
    },
    /* 0x3e */ {},
    /* 0x3f EECR */ {
        "EECR", 0x3f, 0x3f, 0x00,
        make_bits(
            nullptr, nullptr, "EEPM1", "EEPM0",
            "EERIE", "EEMPE", "EEPE", "EERE"),
    },
    /* 0x40 EEDR */ {
        "EEDR", 0xff, 0xff, 0x00,
        make_bits(
            "EEDR7", "EEDR6", "EEDR5", "EEDR4",
            "EEDR3", "EEDR2", "EEDR1", "EEDR0"),
    },
    /* 0x41 EEARL */ {
        "EEARL", 0xff, 0xff, 0x00,
        make_bits(
            "EEAR7", "EEAR6", "EEAR5", "EEAR4",
            "EEAR3", "EEAR2", "EEAR1", "EEAR0"),
    },
    /* 0x42 EEARH */ {
        "EEARH", 0x0f, 0x0f, 0x00,
        make_bits(
            nullptr, nullptr, nullptr, nullptr,
            "EEAR11", "EEAR10", "EEAR9", "EEAR8"),
    },
    /* 0x43 GTCCR */ {
        "GTCCR", 0x83, 0x83, 0x00,
        make_bits(
            "TSM", nullptr, nullptr, nullptr,
            nullptr, nullptr, "PSRASY", "PSRSYNC"),
    },
    /* 0x44 TCCR0A */ {
        "TCCR0A", 0xff, 0xff, 0x00,
        make_bits(
            "COM0A1", "COM0A0", "COM0B1", "COM0B0",
            nullptr, nullptr, "WGM01", "WGM00"),
    },
    /* 0x45 TCCR0B */ {
        "TCCR0B", 0x0f, 0xcf, 0x00,
        make_bits(
            "FOC0A", "FOC0B", nullptr, nullptr,
            "WGM02", "CS02", "CS01", "CS00"),
    },
    /* 0x46 TCNT0 */ {
        "TCNT0", 0xff, 0xff, 0x00,
        make_bits(
            "TCNT0[7]", "TCNT0[6]", "TCNT0[5]", "TCNT0[4]",
            "TCNT0[3]", "TCNT0[2]", "TCNT0[1]", "TCNT0[0]"),
    },
    /* 0x47 OCR0A */ {
        "OCR0A", 0xff, 0xff, 0x00,
        make_bits(
            "OCR0A[7]", "OCR0A[6]", "OCR0A[5]", "OCR0A[4]",
            "OCR0A[3]", "OCR0A[2]", "OCR0A[1]", "OCR0A[0]"),
    },
    /* 0x48 OCR0B */ {
        "OCR0B", 0xff, 0xff, 0x00,
        make_bits(
            "OCR0B[7]", "OCR0B[6]", "OCR0B[5]", "OCR0B[4]",
            "OCR0B[3]", "OCR0B[2]", "OCR0B[1]", "OCR0B[0]"),
    },
    /* 0x49 PLLCSR */ {
        "PLLCSR", 0x13, 0x12, 0x00,
        make_bits(
            nullptr, nullptr, nullptr, "PINDIV",
            nullptr, nullptr, "PLLE", "PLOCK"),
    },
    /* 0x4a GPIOR1 */ {
        "GPIOR1", 0xff, 0xff, 0x00,
        make_bits(
            "GPIOR17", "GPIOR16", "GPIOR15", "GPIOR14",
            "GPIOR13", "GPIOR12", "GPIOR11", "GPIOR10"),
    },
    /* 0x4b GPIOR2 */ {
        "GPIOR2", 0xff, 0xff, 0x00,
        make_bits(
            "GPIOR27", "GPIOR26", "GPIOR25", "GPIOR24",
            "GPIOR23", "GPIOR22", "GPIOR21", "GPIOR20"),
    },
    /* 0x4c SPCR */ {
        "SPCR", 0xff, 0xff, 0x00,
        make_bits(
            "SPIE", "SPE", "DORD", "MSTR",
            "CPOL", "CPHA", "SPR1", "SPR0"),
    },
    /* 0x4d SPSR */ {
        "SPSR", 0xc1, 0x01, 0x00,
        make_bits(
            "SPIF", "WCOL", nullptr, nullptr,
            nullptr, nullptr, nullptr, "SPI2X"),
    },
    /* 0x4e SPDR */ {
        "SPDR", 0xff, 0xff, 0x00,
        make_bits(
            "SPDR7", "SPDR6", "SPDR5", "SPDR4",
            "SPDR3", "SPDR2", "SPDR1", "SPDR0"),
    },
    /* 0x4f */ {},
    /* 0x50 ACSR */ {
        "ACSR", 0xff, 0xdf, 0x00,
        make_bits(
            "ACD", "ACBG", "ACO", "ACI",
            "ACIE", "ACIC", "ACIS1", "ACIS0"),
    },
    /* 0x51 OCDR */ {
        "OCDR/MONDR", 0xff, 0xff, 0x00,
        make_bits(
            "OCDR7", "OCDR6", "OCDR5", "OCDR4",
            "OCDR3", "OCDR2", "OCDR1", "OCDR0"),
    },
    /* 0x52 PLLFRQ */ {
        "PLLFRQ", 0xff, 0xff, 0x04,
        make_bits(
            "PINMUX", "PLLUSB", "PLLTM1", "PLLTM0",
            "PDIV3", "PDIV2", "PDIV1", "PDIV0"),
    },
    /* 0x53 SMCR */ {
        "SMCR", 0x0f, 0x0f, 0x00,
        make_bits(
            nullptr, nullptr, nullptr, nullptr,
            "SM2", "SM1", "SM0", "SE"),
    },
    /* 0x54 MCUSR */ {
        "MCUSR", 0x3f, 0x3f, 0x00,
        make_bits(
            nullptr, nullptr, "USBRF", "JTRF",
            "WDRF", "BORF", "EXTRF", "PORF"),
    },
    /* 0x55 MCUCR */ {
        "MCUCR", 0x93, 0x93, 0x00,
        make_bits(
            "JTD", nullptr, nullptr, "PUD",
            nullptr, nullptr, "IVSEL", "IVCE"),
    },
    /* 0x56 */ {},
    /* 0x57 SPMCSR */ {
        "SPMCSR", 0xff, 0xbf, 0x00,
        make_bits(
            "SPMIE", "RWWSB", "SIGRD", "RWWSRE",
            "BLBSET", "PGWRT", "PGERS", "SPMEN"),
    },
    /* 0x58 */ {},
    /* 0x59 */ {},
    /* 0x5a */ {},
    /* 0x5b RAMPZ */ {
        "RAMPZ", 0x03, 0x03, 0x00,
        make_bits(
            nullptr, nullptr, nullptr, nullptr,
            nullptr, nullptr, "RAMPZ1", "RAMPZ0"),
    },
    /* 0x5c */ {},
    /* 0x5d SPL */ {
        "SPL", 0xff, 0xff, 0xff,
        make_bits(
            "SP7", "SP6", "SP5", "SP4",
            "SP3", "SP2", "SP1", "SP0"),
    },
    /* 0x5e SPH */ {
        "SPH", 0xff, 0xff, 0x0a,
        make_bits(
            "SP15", "SP14", "SP13", "SP12",
            "SP11", "SP10", "SP9", "SP8"),
    },
    /* 0x5f SREG */ {
        "SREG", 0xff, 0xff, 0x00,
        make_bits(
            "I", "T", "H", "S",
            "V", "N", "Z", "C"),
    },
    /* 0x60 WDTCSR */ {
        "WDTCSR", 0xff, 0xff, 0x00,
        make_bits(
            "WDIF", "WDIE", "WDP3", "WDCE",
            "WDE", "WDP2", "WDP1", "WDP0"),
    },
    /* 0x61 CLKPR */ {
        "CLKPR", 0x8f, 0x8f, 0x00,
        make_bits(
            "CLKPCE", nullptr, nullptr, nullptr,
            "CLKPS3", "CLKPS2", "CLKPS1", "CLKPS0"),
    },
    /* 0x62 */ {},
    /* 0x63 */ {},
    /* 0x64 PRR0 */ {
        "PRR0", 0xad, 0xad, 0x00,
        make_bits(
            "PRTWI", nullptr, "PRTIM0", nullptr,
            "PRTIM1", "PRSPI", nullptr, "PRADC"),
    },
    /* 0x65 PRR1 */ {
        "PRR1", 0x99, 0x99, 0x00,
        make_bits(
            "PRUSB", nullptr, nullptr, "PRTIM4",
            "PRTIM3", nullptr, nullptr, "PRUSART1"),
    },
    /* 0x66 OSCCAL */ {
        "OSCCAL", 0xff, 0xff, 0x00,
        make_bits(
            "CAL7", "CAL6", "CAL5", "CAL4",
            "CAL3", "CAL2", "CAL1", "CAL0"),
    },
    /* 0x67 RCCTRL */ {
        "RCCTRL", 0x01, 0x01, 0x00,
        make_bits(
            nullptr, nullptr, nullptr, nullptr,
            nullptr, nullptr, nullptr, "RCFREQ"),
    },
    /* 0x68 PCICR */ {
        "PCICR", 0x01, 0x01, 0x00,
        make_bits(
            nullptr, nullptr, nullptr, nullptr,
            nullptr, nullptr, nullptr, "PCIE0"),
    },
    /* 0x69 EICRA */ {
        "EICRA", 0xff, 0xff, 0x00,
        make_bits(
            "ISC31", "ISC30", "ISC21", "ISC20",
            "ISC11", "ISC10", "ISC01", "ISC00"),
    },
    /* 0x6a EICRB */ {
        "EICRB", 0x30, 0x30, 0x00,
        make_bits(
            nullptr, nullptr, "ISC61", "ISC60",
            nullptr, nullptr, nullptr, nullptr),
    },
    /* 0x6b PCMSK0 */ {
        "PCMSK0", 0xff, 0xff, 0x00,
        make_bits(
            "PCINT7", "PCINT6", "PCINT5", "PCINT4",
            "PCINT3", "PCINT2", "PCINT1", "PCINT0"),
    },
    /* 0x6c */ {},
    /* 0x6d */ {},
    /* 0x6e TIMSK0 */ {
        "TIMSK0", 0x07, 0x07, 0x00,
        make_bits(
            nullptr, nullptr, nullptr, nullptr,
            nullptr, "OCIE0B", "OCIE0A", "TOIE0"),
    },
    /* 0x6f TIMSK1 */ {
        "TIMSK1", 0x2f, 0x2f, 0x00,
        make_bits(
            nullptr, nullptr, "ICIE1", nullptr,
            "OCIE1C", "OCIE1B", "OCIE1A", "TOIE1"),
    },
    /* 0x70 */ {},
    /* 0x71 TIMSK3 */ {
        "TIMSK3", 0x2f, 0x2f, 0x00,
        make_bits(
            nullptr, nullptr, "ICIE3", nullptr,
            "OCIE3C", "OCIE3B", "OCIE3A", "TOIE3"),
    },
    /* 0x72 TIMSK4 */ {
        "TIMSK4", 0xe4, 0xe4, 0x00,
        make_bits(
            "OCIE4D", "OCIE4A", "OCIE4B", nullptr,
            nullptr, "TOIE4", nullptr, nullptr),
    },
    /* 0x73 */ {},
    /* 0x74 */ {},
    /* 0x75 */ {},
    /* 0x76 */ {},
    /* 0x77 */ {},
    /* 0x78 ADCL */ {
        "ADCL", 0xff, 0x00, 0x00,
        make_bits(
            "ADC[7]", "ADC[6]", "ADC[5]", "ADC[4]",
            "ADC[3]", "ADC[2]", "ADC[1]", "ADC[0]"),
    },
    /* 0x79 ADCH */ {
        "ADCH", 0xff, 0x00, 0x00,
        make_bits(
            "ADC high", "ADC high", "ADC high", "ADC high",
            "ADC high", "ADC high", "ADC high", "ADC high"),
    },
    /* 0x7a ADCSRA */ {
        "ADCSRA", 0xff, 0xff, 0x00,
        make_bits(
            "ADEN", "ADSC", "ADATE", "ADIF",
            "ADIE", "ADPS2", "ADPS1", "ADPS0"),
    },
    /* 0x7b ADCSRB */ {
        "ADCSRB", 0xef, 0x6f, 0x00,
        make_bits(
            "ADHSM", "ACME", "MUX5", nullptr,
            "ADTS3", "ADTS2", "ADTS1", "ADTS0"),
    },
    /* 0x7c ADMUX */ {
        "ADMUX", 0xff, 0xff, 0x00,
        make_bits(
            "REFS1", "REFS0", "ADLAR", "MUX4",
            "MUX3", "MUX2", "MUX1", "MUX0"),
    },
    /* 0x7d DIDR2 */ {
        "DIDR2", 0x3f, 0x3f, 0x00,
        make_bits(
            "ADC13D", "ADC12D", "ADC11D", "ADC10D",
            "ADC9D", "ADC8D", nullptr, nullptr),
    },
    /* 0x7e DIDR0 */ {
        "DIDR0", 0xf3, 0xf3, 0x00,
        make_bits(
            "ADC7D", "ADC6D", "ADC5D", "ADC4D",
            nullptr, nullptr, "ADC1D", "ADC0D"),
    },
    /* 0x7f DIDR1 */ {
        "DIDR1", 0x01, 0x01, 0x00,
        make_bits(
            nullptr, nullptr, nullptr, nullptr,
            nullptr, nullptr, nullptr, "AIN0D"),
    },
    /* 0x80 TCCR1A */ {
        "TCCR1A", 0xff, 0xff, 0x00,
        make_bits(
            "COM1A1", "COM1A0", "COM1B1", "COM1B0",
            "COM1C1", "COM1C0", "WGM11", "WGM10"),
    },
    /* 0x81 TCCR1B */ {
        "TCCR1B", 0xdf, 0xdf, 0x00,
        make_bits(
            "ICNC1", "ICES1", nullptr, "WGM13",
            "WGM12", "CS12", "CS11", "CS10"),
    },
    /* 0x82 TCCR1C */ {
        "TCCR1C", 0x00, 0xe0, 0x00,
        make_bits(
            "FOC1A", "FOC1B", "FOC1C", nullptr,
            nullptr, nullptr, nullptr, nullptr),
    },
    /* 0x83 */ {},
    /* 0x84 TCNT1L */ {
        "TCNT1L", 0xff, 0xff, 0x00,
        make_bits(
            "TCNT1[7]", "TCNT1[6]", "TCNT1[5]", "TCNT1[4]",
            "TCNT1[3]", "TCNT1[2]", "TCNT1[1]", "TCNT1[0]"),
    },
    /* 0x85 TCNT1H */ {
        "TCNT1H", 0xff, 0xff, 0x00,
        make_bits(
            "TCNT1[15]", "TCNT1[14]", "TCNT1[13]", "TCNT1[12]",
            "TCNT1[11]", "TCNT1[10]", "TCNT1[9]", "TCNT1[8]"),
    },
    /* 0x86 ICR1L */ {
        "ICR1L", 0xff, 0xff, 0x00,
        make_bits(
            "ICR1[7]", "ICR1[6]", "ICR1[5]", "ICR1[4]",
            "ICR1[3]", "ICR1[2]", "ICR1[1]", "ICR1[0]"),
    },
    /* 0x87 ICR1H */ {
        "ICR1H", 0xff, 0xff, 0x00,
        make_bits(
            "ICR1[15]", "ICR1[14]", "ICR1[13]", "ICR1[12]",
            "ICR1[11]", "ICR1[10]", "ICR1[9]", "ICR1[8]"),
    },
    /* 0x88 OCR1AL */ {
        "OCR1AL", 0xff, 0xff, 0x00,
        make_bits(
            "OCR1A[7]", "OCR1A[6]", "OCR1A[5]", "OCR1A[4]",
            "OCR1A[3]", "OCR1A[2]", "OCR1A[1]", "OCR1A[0]"),
    },
    /* 0x89 OCR1AH */ {
        "OCR1AH", 0xff, 0xff, 0x00,
        make_bits(
            "OCR1A[15]", "OCR1A[14]", "OCR1A[13]", "OCR1A[12]",
            "OCR1A[11]", "OCR1A[10]", "OCR1A[9]", "OCR1A[8]"),
    },
    /* 0x8a OCR1BL */ {
        "OCR1BL", 0xff, 0xff, 0x00,
        make_bits(
            "OCR1B[7]", "OCR1B[6]", "OCR1B[5]", "OCR1B[4]",
            "OCR1B[3]", "OCR1B[2]", "OCR1B[1]", "OCR1B[0]"),
    },
    /* 0x8b OCR1BH */ {
        "OCR1BH", 0xff, 0xff, 0x00,
        make_bits(
            "OCR1B[15]", "OCR1B[14]", "OCR1B[13]", "OCR1B[12]",
            "OCR1B[11]", "OCR1B[10]", "OCR1B[9]", "OCR1B[8]"),
    },
    /* 0x8c OCR1CL */ {
        "OCR1CL", 0xff, 0xff, 0x00,
        make_bits(
            "OCR1C[7]", "OCR1C[6]", "OCR1C[5]", "OCR1C[4]",
            "OCR1C[3]", "OCR1C[2]", "OCR1C[1]", "OCR1C[0]"),
    },
    /* 0x8d OCR1CH */ {
        "OCR1CH", 0xff, 0xff, 0x00,
        make_bits(
            "OCR1C[15]", "OCR1C[14]", "OCR1C[13]", "OCR1C[12]",
            "OCR1C[11]", "OCR1C[10]", "OCR1C[9]", "OCR1C[8]"),
    },
    /* 0x8e */ {},
    /* 0x8f */ {},
    /* 0x90 TCCR3A */ {
        "TCCR3A", 0xff, 0xff, 0x00,
        make_bits(
            "COM3A1", "COM3A0", "COM3B1", "COM3B0",
            "COM3C1", "COM3C0", "WGM31", "WGM30"),
    },
    /* 0x91 TCCR3B */ {
        "TCCR3B", 0xdf, 0xdf, 0x00,
        make_bits(
            "ICNC3", "ICES3", nullptr, "WGM33",
            "WGM32", "CS32", "CS31", "CS30"),
    },
    /* 0x92 TCCR3C */ {
        "TCCR3C", 0x00, 0x80, 0x00,
        make_bits(
            "FOC3A", nullptr, nullptr, nullptr,
            nullptr, nullptr, nullptr, nullptr),
    },
    /* 0x93 */ {},
    /* 0x94 TCNT3L */ {
        "TCNT3L", 0xff, 0xff, 0x00,
        make_bits(
            "TCNT3[7]", "TCNT3[6]", "TCNT3[5]", "TCNT3[4]",
            "TCNT3[3]", "TCNT3[2]", "TCNT3[1]", "TCNT3[0]"),
    },
    /* 0x95 TCNT3H */ {
        "TCNT3H", 0xff, 0xff, 0x00,
        make_bits(
            "TCNT3[15]", "TCNT3[14]", "TCNT3[13]", "TCNT3[12]",
            "TCNT3[11]", "TCNT3[10]", "TCNT3[9]", "TCNT3[8]"),
    },
    /* 0x96 ICR3L */ {
        "ICR3L", 0xff, 0xff, 0x00,
        make_bits(
            "ICR3[7]", "ICR3[6]", "ICR3[5]", "ICR3[4]",
            "ICR3[3]", "ICR3[2]", "ICR3[1]", "ICR3[0]"),
    },
    /* 0x97 ICR3H */ {
        "ICR3H", 0xff, 0xff, 0x00,
        make_bits(
            "ICR3[15]", "ICR3[14]", "ICR3[13]", "ICR3[12]",
            "ICR3[11]", "ICR3[10]", "ICR3[9]", "ICR3[8]"),
    },
    /* 0x98 OCR3AL */ {
        "OCR3AL", 0xff, 0xff, 0x00,
        make_bits(
            "OCR3A[7]", "OCR3A[6]", "OCR3A[5]", "OCR3A[4]",
            "OCR3A[3]", "OCR3A[2]", "OCR3A[1]", "OCR3A[0]"),
    },
    /* 0x99 OCR3AH */ {
        "OCR3AH", 0xff, 0xff, 0x00,
        make_bits(
            "OCR3A[15]", "OCR3A[14]", "OCR3A[13]", "OCR3A[12]",
            "OCR3A[11]", "OCR3A[10]", "OCR3A[9]", "OCR3A[8]"),
    },
    /* 0x9a OCR3BL */ {
        "OCR3BL", 0xff, 0xff, 0x00,
        make_bits(
            "OCR3B[7]", "OCR3B[6]", "OCR3B[5]", "OCR3B[4]",
            "OCR3B[3]", "OCR3B[2]", "OCR3B[1]", "OCR3B[0]"),
    },
    /* 0x9b OCR3BH */ {
        "OCR3BH", 0xff, 0xff, 0x00,
        make_bits(
            "OCR3B[15]", "OCR3B[14]", "OCR3B[13]", "OCR3B[12]",
            "OCR3B[11]", "OCR3B[10]", "OCR3B[9]", "OCR3B[8]"),
    },
    /* 0x9c OCR3CL */ {
        "OCR3CL", 0xff, 0xff, 0x00,
        make_bits(
            "OCR3C[7]", "OCR3C[6]", "OCR3C[5]", "OCR3C[4]",
            "OCR3C[3]", "OCR3C[2]", "OCR3C[1]", "OCR3C[0]"),
    },
    /* 0x9d OCR3CH */ {
        "OCR3CH", 0xff, 0xff, 0x00,
        make_bits(
            "OCR3C[15]", "OCR3C[14]", "OCR3C[13]", "OCR3C[12]",
            "OCR3C[11]", "OCR3C[10]", "OCR3C[9]", "OCR3C[8]"),
    },
    /* 0x9e */ {},
    /* 0x9f */ {},
    /* 0xa0 */ {},
    /* 0xa1 */ {},
    /* 0xa2 */ {},
    /* 0xa3 */ {},
    /* 0xa4 */ {},
    /* 0xa5 */ {},
    /* 0xa6 */ {},
    /* 0xa7 */ {},
    /* 0xa8 */ {},
    /* 0xa9 */ {},
    /* 0xaa */ {},
    /* 0xab */ {},
    /* 0xac */ {},
    /* 0xad */ {},
    /* 0xae */ {},
    /* 0xaf */ {},
    /* 0xb0 */ {},
    /* 0xb1 */ {},
    /* 0xb2 */ {},
    /* 0xb3 */ {},
    /* 0xb4 */ {},
    /* 0xb5 */ {},
    /* 0xb6 */ {},
    /* 0xb7 */ {},
    /* 0xb8 TWBR */ {
        "TWBR", 0xff, 0xff, 0x00,
        make_bits(
            "TWBR7", "TWBR6", "TWBR5", "TWBR4",
            "TWBR3", "TWBR2", "TWBR1", "TWBR0"),
    },
    /* 0xb9 TWSR */ {
        "TWSR", 0xfb, 0x03, 0xf8,
        make_bits(
            "TWS7", "TWS6", "TWS5", "TWS4",
            "TWS3", nullptr, "TWPS1", "TWPS0"),
    },
    /* 0xba TWAR */ {
        "TWAR", 0xff, 0xff, 0xfe,
        make_bits(
            "TWA6", "TWA5", "TWA4", "TWA3",
            "TWA2", "TWA1", "TWA0", "TWGCE"),
    },
    /* 0xbb TWDR */ {
        "TWDR", 0xff, 0xff, 0xff,
        make_bits(
            "TWD7", "TWD6", "TWD5", "TWD4",
            "TWD3", "TWD2", "TWD1", "TWD0"),
    },
    /* 0xbc TWCR */ {
        "TWCR", 0xfd, 0xf5, 0x00,
        make_bits(
            "TWINT", "TWEA", "TWSTA", "TWSTO",
            "TWWC", "TWEN", nullptr, "TWIE"),
    },
    /* 0xbd TWAMR */ {
        "TWAMR", 0xfe, 0xfe, 0x00,
        make_bits(
            nullptr, "TWAM0", "TWAM1", "TWAM2",
            "TWAM3", "TWAM4", "TWAM5", "TWAM6"),
    },
    /* 0xbe TCNT4 */ {
        "TCNT4", 0xff, 0xff, 0x00,
        make_bits(
            "TCNT4[7]", "TCNT4[6]", "TCNT4[5]", "TCNT4[4]",
            "TCNT4[3]", "TCNT4[2]", "TCNT4[1]", "TCNT4[0]"),
    },
    /* 0xbf TC4H */ {
        "TC4H", 0x07, 0x07, 0x00,
        make_bits(
            nullptr, nullptr, nullptr, nullptr,
            nullptr, "TC4H2", "TC4H1", "TC4H0"),
    },
    /* 0xc0 TCCR4A */ {
        "TCCR4A", 0xf3, 0xff, 0x00,
        make_bits(
            "COM4A1", "COM4A0", "COM4B1", "COM4B0",
            "FOC4A", "FOC4B", "PWM4A", "PWM4B"),
    },
    /* 0xc1 TCCR4B */ {
        "TCCR4B", 0xff, 0xff, 0x00,
        make_bits(
            "PWM4X", "PSR4", "DTPS41", "DTPS40",
            "CS43", "CS42", "CS41", "CS40"),
    },
    /* 0xc2 TCCR4C */ {
        "TCCR4C", 0xfd, 0xff, 0x00,
        make_bits(
            "COM4A1S", "COM4A0S", "COM4B1S", "COM4B0S",
            "COM4D1S", "COM4D0S", "FOC4D", "PWM4D"),
    },
    /* 0xc3 TCCR4D */ {
        "TCCR4D", 0xff, 0xff, 0x00,
        make_bits(
            "FPIE4", "FPEN4", "FPNC4", "FPES4",
            "FPAC4", "FPF4", "WGM41", "WGM40"),
    },
    /* 0xc4 TCCR4E */ {
        "TCCR4E", 0xff, 0xff, 0x00,
        make_bits(
            "TLOCK4", "ENHC4", "OC4OE5", "OC4OE4",
            "OC4OE3", "OC4OE2", "OC4OE1", "OC4OE0"),
    },
    /* 0xc5 CLKSEL0 */ {
        "CLKSEL0", 0xfd, 0xfd, 0x00,
        make_bits(
            "RCSUT1", "RCSUT0", "EXSUT1", "EXSUT0",
            "RCE", "EXTE", nullptr, "CLKS"),
    },
    /* 0xc6 CLKSEL1 */ {
        "CLKSEL1", 0xff, 0xff, 0x20,
        make_bits(
            "RCCKSEL3", "RCCKSEL2", "RCCKSEL1", "RCCKSEL0",
            "EXCKSEL3", "EXCKSEL2", "EXCKSEL1", "EXCKSEL0"),
    },
    /* 0xc7 CLKSTA */ {
        "CLKSTA", 0x03, 0x00, 0x00,
        make_bits(
            nullptr, nullptr, nullptr, nullptr,
            nullptr, nullptr, "RCON", "EXTON"),
    },
    /* 0xc8 UCSR1A */ {
        "UCSR1A", 0xff, 0x43, 0x20,
        make_bits(
            "RXC1", "TXC1", "UDRE1", "FE1",
            "DOR1", "PE1", "U2X1", "MPCM1"),
    },
    /* 0xc9 UCSR1B */ {
        "UCSR1B", 0xff, 0xff, 0x00,
        make_bits(
            "RXCIE1", "TXCIE1", "UDRIE1", "RXEN1",
            "TXEN1", "UCSZ12", "RXB81", "TXB81"),
    },
    /* 0xca UCSR1C */ {
        "UCSR1C", 0xff, 0xff, 0x06,
        make_bits(
            "UMSEL11", "UMSEL10", "UPM11", "UPM10",
            "USBS1", "UCSZ11", "UCSZ10", "UCPOL1"),
    },
    /* 0xcb UCSR1D */ {
        "UCSR1D", 0x03, 0x03, 0x00,
        make_bits(
            nullptr, nullptr, nullptr, nullptr,
            nullptr, nullptr, "CTSEN", "RTSEN"),
    },
    /* 0xcc UBRR1L */ {
        "UBRR1L", 0xff, 0xff, 0x00,
        make_bits(
            "UBRR1[7]", "UBRR1[6]", "UBRR1[5]", "UBRR1[4]",
            "UBRR1[3]", "UBRR1[2]", "UBRR1[1]", "UBRR1[0]"),
    },
    /* 0xcd UBRR1H */ {
        "UBRR1H", 0x0f, 0x0f, 0x00,
        make_bits(
            nullptr, nullptr, nullptr, nullptr,
            "UBRR1[11]", "UBRR1[10]", "UBRR1[9]", "UBRR1[8]"),
    },
    /* 0xce UDR1 */ {
        "UDR1", 0xff, 0xff, 0x00,
        make_bits(
            "UDR1[7]", "UDR1[6]", "UDR1[5]", "UDR1[4]",
            "UDR1[3]", "UDR1[2]", "UDR1[1]", "UDR1[0]"),
    },
    /* 0xcf OCR4A */ {
        "OCR4A", 0xff, 0xff, 0x00,
        make_bits(
            "OCR4A7", "OCR4A6", "OCR4A5", "OCR4A4",
            "OCR4A3", "OCR4A2", "OCR4A1", "OCR4A0"),
    },
    /* 0xd0 OCR4B */ {
        "OCR4B", 0xff, 0xff, 0x00,
        make_bits(
            "OCR4B7", "OCR4B6", "OCR4B5", "OCR4B4",
            "OCR4B3", "OCR4B2", "OCR4B1", "OCR4B0"),
    },
    /* 0xd1 OCR4C */ {
        "OCR4C", 0xff, 0xff, 0x00,
        make_bits(
            "OCR4C7", "OCR4C6", "OCR4C5", "OCR4C4",
            "OCR4C3", "OCR4C2", "OCR4C1", "OCR4C0"),
    },
    /* 0xd2 OCR4D */ {
        "OCR4D", 0xff, 0xff, 0x00,
        make_bits(
            "OCR4D7", "OCR4D6", "OCR4D5", "OCR4D4",
            "OCR4D3", "OCR4D2", "OCR4D1", "OCR4D0"),
    },
    /* 0xd3 */ {},
    /* 0xd4 DT4 */ {
        "DT4", 0xff, 0xff, 0x00,
        make_bits(
            "DT4H3", "DT4H2", "DT4H1", "DT4H0",
            "DT4L3", "DT4L2", "DT4L1", "DT4L0"),
    },
    /* 0xd5 */ {},
    /* 0xd6 */ {},
    /* 0xd7 UHWCON */ {
        "UHWCON", 0x01, 0x01, 0x00,
        make_bits(
            nullptr, nullptr, nullptr, nullptr,
            nullptr, nullptr, nullptr, "UVREGE"),
    },
    /* 0xd8 USBCON */ {
        "USBCON", 0xb1, 0xb1, 0x20,
        make_bits(
            "USBE", nullptr, "FRZCLK", "OTGPADE",
            nullptr, nullptr, nullptr, "VBUSTE"),
    },
    /* 0xd9 USBSTA */ {
        "USBSTA", 0x03, 0x00, 0x00,
        make_bits(
            nullptr, nullptr, nullptr, nullptr,
            nullptr, nullptr, "ID", "VBUS"),
    },
    /* 0xda USBINT */ {
        "USBINT", 0x01, 0x01, 0x00,
        make_bits(
            nullptr, nullptr, nullptr, nullptr,
            nullptr, nullptr, nullptr, "VBUSTI"),
    },
    /* 0xdb */ {},
    /* 0xdc */ {},
    /* 0xdd */ {},
    /* 0xde */ {},
    /* 0xdf */ {},
    /* 0xe0 UDCON */ {
        "UDCON", 0x0f, 0x0f, 0x00,
        make_bits(
            nullptr, nullptr, nullptr, nullptr,
            "RSTCPU", "LSM", "RMWKUP", "DETACH"),
    },
    /* 0xe1 UDINT */ {
        "UDINT", 0x7f, 0x7f, 0x00,
        make_bits(
            nullptr, "UPRSMI", "EORSMI", "WAKEUPI",
            "EORSTI", "SOFI", "MSOFI", "SUSPI"),
    },
    /* 0xe2 UDIEN */ {
        "UDIEN", 0x7f, 0x7f, 0x00,
        make_bits(
            nullptr, "UPRSME", "EORSME", "WAKEUPE",
            "EORSTE", "SOFE", "MSOFE", "SUSPE"),
    },
    /* 0xe3 UDADDR */ {
        "UDADDR", 0xff, 0xff, 0x00,
        make_bits(
            "ADDEN", "UADD6", "UADD5", "UADD4",
            "UADD3", "UADD2", "UADD1", "UADD0"),
    },
    /* 0xe4 UDFNUML */ {
        "UDFNUML", 0xff, 0x00, 0x00,
        make_bits(
            "FNUM7", "FNUM6", "FNUM5", "FNUM4",
            "FNUM3", "FNUM2", "FNUM1", "FNUM0"),
    },
    /* 0xe5 UDFNUMH */ {
        "UDFNUMH", 0x07, 0x00, 0x00,
        make_bits(
            nullptr, nullptr, nullptr, nullptr,
            nullptr, "FNUM10", "FNUM9", "FNUM8"),
    },
    /* 0xe6 UDMFN */ {
        "UDMFN", 0x10, 0x10, 0x00,
        make_bits(
            nullptr, nullptr, nullptr, "FNCERR",
            nullptr, nullptr, nullptr, nullptr),
    },
    /* 0xe7 */ {},
    /* 0xe8 UEINTX */ {
        "UEINTX", 0xff, 0xdf, 0x00,
        make_bits(
            "FIFOCON", "NAKINI", "RWAL", "NAKOUTI",
            "RXSTPI", "RXOUTI", "STALLEDI", "TXINI"),
    },
    /* 0xe9 UENUM */ {
        "UENUM", 0x07, 0x07, 0x00,
        make_bits(
            nullptr, nullptr, nullptr, nullptr,
            nullptr, "EPNUM2", "EPNUM1", "EPNUM0"),
    },
    /* 0xea UERST */ {
        "UERST", 0x7f, 0x7f, 0x00,
        make_bits(
            nullptr, "EPRST6", "EPRST5", "EPRST4",
            "EPRST3", "EPRST2", "EPRST1", "EPRST0"),
    },
    /* 0xeb UECONX */ {
        "UECONX", 0x39, 0x39, 0x00,
        make_bits(
            nullptr, nullptr, nullptr, "STALLRQ",
            "STALLRQC", "RSTDT", nullptr, "EPEN"),
    },
    /* 0xec UECFG0X */ {
        "UECFG0X", 0xc1, 0xc1, 0x00,
        make_bits(
            "EPTYPE1", "EPTYPE0", nullptr, nullptr,
            nullptr, nullptr, nullptr, "EPDIR"),
    },
    /* 0xed UECFG1X */ {
        "UECFG1X", 0xfc, 0xfc, 0x00,
        make_bits(
            "EPSIZE2", "EPSIZE1", "EPSIZE0", "EPBK1",
            "EPBK0", "ALLOC", nullptr, nullptr),
    },
    /* 0xee UESTA0X */ {
        "UESTA0X", 0xef, 0x60, 0x00,
        make_bits(
            "CFGOK", "OVERFI", "UNDERFI", nullptr,
            "DTSEQ1", "DTSEQ0", "NBUSYBK1", "NBUSYBK0"),
    },
    /* 0xef UESTA1X */ {
        "UESTA1X", 0x07, 0x00, 0x00,
        make_bits(
            nullptr, nullptr, nullptr, nullptr,
            nullptr, "CTRLDIR", "CURRBK1", "CURRBK0"),
    },
    /* 0xf0 UEIENX */ {
        "UEIENX", 0xdf, 0xdf, 0x00,
        make_bits(
            "FLERRE", "NAKINE", nullptr, "NAKOUTE",
            "RXSTPE", "RXOUTE", "STALLEDE", "TXINE"),
    },
    /* 0xf1 UEDATX */ {
        "UEDATX", 0xff, 0xff, 0x00,
        make_bits(
            "DAT7", "DAT6", "DAT5", "DAT4",
            "DAT3", "DAT2", "DAT1", "DAT0"),
    },
    /* 0xf2 UEBCLX */ {
        "UEBCLX", 0xff, 0x00, 0x00,
        make_bits(
            "BYCT7", "BYCT6", "BYCT5", "BYCT4",
            "BYCT3", "BYCT2", "BYCT1", "BYCT0"),
    },
    /* 0xf3 UEBCHX */ {
        "UEBCHX", 0x07, 0x00, 0x00,
        make_bits(
            nullptr, nullptr, nullptr, nullptr,
            nullptr, "BYCT10", "BYCT9", "BYCT8"),
    },
    /* 0xf4 UEINT */ {
        "UEINT", 0x7f, 0x00, 0x00,
        make_bits(
            nullptr, "EPINT6", "EPINT5", "EPINT4",
            "EPINT3", "EPINT2", "EPINT1", "EPINT0"),
    },
    /* 0xf5 */ {},
    /* 0xf6 */ {},
    /* 0xf7 */ {},
    /* 0xf8 */ {},
    /* 0xf9 */ {},
    /* 0xfa */ {},
    /* 0xfb */ {},
    /* 0xfc */ {},
    /* 0xfd */ {},
    /* 0xfe */ {},
    /* 0xff */ {},
}};


} // namespace reg
} // namespace absim
