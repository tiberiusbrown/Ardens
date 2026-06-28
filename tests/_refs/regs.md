Below is the **ATmega32U4 CPU/special-purpose register map**, not the full peripheral SFR map. It covers the CPU register file aliases, SREG, SP, RAMPZ, GPIOR0–2, and PC. The ATmega32U4 datasheet maps the 32 working registers at data addresses `0x0000–0x001F`, standard I/O at data addresses `0x0020–0x005F`, and extended I/O at `0x0060–0x00FF`; for standard I/O registers, the `IN/OUT` I/O address is `data address - 0x20`. ([Microchip][1])

For reset: all I/O registers are set to their documented initial values during reset, while the CPU starts execution from the reset vector. The stack pointer reset value is the last internal SRAM address; for **ATmega32U4**, SRAM ends at `0x0AFF`, so `SPH:SPL = 0x0A:0xFF`. ([Microchip][1])

### Access legend

`R/W` means software-readable and software-writable. `R` means read-only from software. `R0` means reserved/unimplemented, reads as zero, and should be written as zero. `U` means not specified/undefined by reset.

| Register   | I/O addr |   Data addr | Bits 7..0 / fields                                | Bit access 7..0             |  Reset value |
| ---------- | -------: | ----------: | ------------------------------------------------- | --------------------------- | -----------: |
| `R0–R25`   |        — | `0x00–0x19` | `Rn[7:0]`                                         | `R/W` all bits              |          `U` |
| `R26 / XL` |        — |      `0x1A` | `XL7 XL6 XL5 XL4 XL3 XL2 XL1 XL0`                 | `R/W` all bits              |          `U` |
| `R27 / XH` |        — |      `0x1B` | `XH7 XH6 XH5 XH4 XH3 XH2 XH1 XH0`                 | `R/W` all bits              |          `U` |
| `R28 / YL` |        — |      `0x1C` | `YL7 YL6 YL5 YL4 YL3 YL2 YL1 YL0`                 | `R/W` all bits              |          `U` |
| `R29 / YH` |        — |      `0x1D` | `YH7 YH6 YH5 YH4 YH3 YH2 YH1 YH0`                 | `R/W` all bits              |          `U` |
| `R30 / ZL` |        — |      `0x1E` | `ZL7 ZL6 ZL5 ZL4 ZL3 ZL2 ZL1 ZL0`                 | `R/W` all bits              |          `U` |
| `R31 / ZH` |        — |      `0x1F` | `ZH7 ZH6 ZH5 ZH4 ZH3 ZH2 ZH1 ZH0`                 | `R/W` all bits              |          `U` |
| `GPIOR0`   |   `0x1E` |      `0x3E` | `GPIOR0[7:0]`                                     | `R/W` all bits              |       `0x00` |
| `GPIOR1`   |   `0x2A` |      `0x4A` | `GPIOR1[7:0]`                                     | `R/W` all bits              |       `0x00` |
| `GPIOR2`   |   `0x2B` |      `0x4B` | `GPIOR2[7:0]`                                     | `R/W` all bits              |       `0x00` |
| `RAMPZ`    |   `0x3B` |      `0x5B` | `— — — — — — RAMPZ1 RAMPZ0`                       | `R0 R0 R0 R0 R0 R0 R/W R/W` |       `0x00` |
| `SPL`      |   `0x3D` |      `0x5D` | `SP7 SP6 SP5 SP4 SP3 SP2 SP1 SP0`                 | `R/W` all bits              |       `0xFF` |
| `SPH`      |   `0x3E` |      `0x5E` | `SP15 SP14 SP13 SP12 SP11 SP10 SP9 SP8`           | `R/W` all bits              |       `0x0A` |
| `SREG`     |   `0x3F` |      `0x5F` | `I T H S V N Z C`                                 | `R/W` all bits              |       `0x00` |
| `PC`       |        — |           — | `PC[13:0]` for ATmega32U4 program word addressing | not memory-mapped           | reset vector |

The register-summary table lists `SREG`, `SPH`, `SPL`, and `RAMPZ` at I/O addresses `0x3F`, `0x3E`, `0x3D`, and `0x3B` respectively, with the corresponding data-space addresses in parentheses. It also lists `GPIOR0`, `GPIOR1`, and `GPIOR2` at `0x1E`, `0x2A`, and `0x2B`. ([Microchip][1])

### Field notes for emulator behavior

`SREG` is the ALU/status register. Its bits are `I` global interrupt enable, `T` bit-copy storage, `H` half carry, `S` sign, `V` two’s-complement overflow, `N` negative, `Z` zero, and `C` carry. The datasheet marks every SREG bit as `R/W` with reset value zero; ALU instructions update the arithmetic flags, interrupt entry clears `I`, and `RETI` sets `I`. ([Microchip][1])

`SPH:SPL` is a 16-bit stack pointer in I/O space. On ATmega32U4 reset, initialize it to `0x0AFF`. Push/call/interrupt operations decrement it before or while storing stack bytes; pop/return operations increment it while reading stack bytes. ([Microchip][1])

`X`, `Y`, and `Z` are not separate storage from the register file. They are aliases: `X = R27:R26`, `Y = R29:R28`, and `Z = R31:R30`. They reset as undefined like the rest of the working register file, unless your emulator intentionally initializes CPU registers deterministically for debugging. 

`RAMPZ` extends the Z pointer for `ELPM`/`SPM`-class program-memory operations. The CPU section says unused RAMPZ bits read zero and should be written zero; the ATmega32U4 register summary exposes only `RAMPZ1:RAMPZ0`. ([Microchip][1])

`PC` is architectural, not memory-mapped. For ATmega32U4 self-programming tables, the program counter is described as `PC[13:0]`. At reset, execution starts from address `0x0000` unless the boot-reset fuse or hardware boot condition redirects to the bootloader reset address. ([Microchip][1])

Below is the **defined peripheral/system SFR map** for the **ATmega32U4**, excluding the CPU-core registers already covered earlier (`R0–R31`, `SREG`, `SPH/SPL`, `RAMPZ`, `PC`). Reserved addresses are omitted. The datasheet’s register summary gives the bit names and addresses; standard I/O registers have both an I/O address and a data-space address, while extended I/O registers at data address `0x60+` are only accessible with data-space load/store instructions. ([Microchip][1])

Legend: `R0` means reserved/read-as-zero; `R/W*` means the datasheet marks the bit R/W but the bit has hardware/set/clear side effects; `N/A` means reset value is pin/state dependent or not a real storage reset value.

## Standard I/O peripheral SFRs

|    I/O |   Data | Name         | Bits 7 → 0                                                | Access 7 → 0                          |        Reset |
| -----: | -----: | ------------ | --------------------------------------------------------- | ------------------------------------- | -----------: |
| `0x03` | `0x23` | `PINB`       | `PINB7 PINB6 PINB5 PINB4 PINB3 PINB2 PINB1 PINB0`         | `R/W* ×8`                             |        `N/A` |
| `0x04` | `0x24` | `DDRB`       | `DDB7 DDB6 DDB5 DDB4 DDB3 DDB2 DDB1 DDB0`                 | `R/W ×8`                              |       `0x00` |
| `0x05` | `0x25` | `PORTB`      | `PORTB7 PORTB6 PORTB5 PORTB4 PORTB3 PORTB2 PORTB1 PORTB0` | `R/W ×8`                              |       `0x00` |
| `0x06` | `0x26` | `PINC`       | `PINC7 PINC6 - - - - - -`                                 | `R/W* R/W* R0 R0 R0 R0 R0 R0`         |        `N/A` |
| `0x07` | `0x27` | `DDRC`       | `DDC7 DDC6 - - - - - -`                                   | `R/W R/W R0 R0 R0 R0 R0 R0`           |       `0x00` |
| `0x08` | `0x28` | `PORTC`      | `PORTC7 PORTC6 - - - - - -`                               | `R/W R/W R0 R0 R0 R0 R0 R0`           |       `0x00` |
| `0x09` | `0x29` | `PIND`       | `PIND7 PIND6 PIND5 PIND4 PIND3 PIND2 PIND1 PIND0`         | `R/W* ×8`                             |        `N/A` |
| `0x0A` | `0x2A` | `DDRD`       | `DDD7 DDD6 DDD5 DDD4 DDD3 DDD2 DDD1 DDD0`                 | `R/W ×8`                              |       `0x00` |
| `0x0B` | `0x2B` | `PORTD`      | `PORTD7 PORTD6 PORTD5 PORTD4 PORTD3 PORTD2 PORTD1 PORTD0` | `R/W ×8`                              |       `0x00` |
| `0x0C` | `0x2C` | `PINE`       | `- PINE6 - - - PINE2 - -`                                 | `R0 R/W* R0 R0 R0 R/W* R0 R0`         |        `N/A` |
| `0x0D` | `0x2D` | `DDRE`       | `- DDE6 - - - DDE2 - -`                                   | `R0 R/W R0 R0 R0 R/W R0 R0`           |       `0x00` |
| `0x0E` | `0x2E` | `PORTE`      | `- PORTE6 - - - PORTE2 - -`                               | `R0 R/W R0 R0 R0 R/W R0 R0`           |       `0x00` |
| `0x0F` | `0x2F` | `PINF`       | `PINF7 PINF6 PINF5 PINF4 - - PINF1 PINF0`                 | `R/W* R/W* R/W* R/W* R0 R0 R/W* R/W*` |        `N/A` |
| `0x10` | `0x30` | `DDRF`       | `DDF7 DDF6 DDF5 DDF4 - - DDF1 DDF0`                       | `R/W R/W R/W R/W R0 R0 R/W R/W`       |       `0x00` |
| `0x11` | `0x31` | `PORTF`      | `PORTF7 PORTF6 PORTF5 PORTF4 - - PORTF1 PORTF0`           | `R/W R/W R/W R/W R0 R0 R/W R/W`       |       `0x00` |
| `0x15` | `0x35` | `TIFR0`      | `- - - - - OCF0B OCF0A TOV0`                              | `R0 R0 R0 R0 R0 R/W* R/W* R/W*`       |       `0x00` |
| `0x16` | `0x36` | `TIFR1`      | `- - ICF1 - OCF1C OCF1B OCF1A TOV1`                       | `R0 R0 R/W* R0 R/W* R/W* R/W* R/W*`   |       `0x00` |
| `0x18` | `0x38` | `TIFR3`      | `- - ICF3 - OCF3C OCF3B OCF3A TOV3`                       | `R0 R0 R/W* R0 R/W* R/W* R/W* R/W*`   |       `0x00` |
| `0x19` | `0x39` | `TIFR4`      | `OCF4D OCF4A OCF4B - - TOV4 - -`                          | `R/W* R/W* R/W* R0 R0 R/W* R0 R0`     |       `0x00` |
| `0x1B` | `0x3B` | `PCIFR`      | `- - - - - - - PCIF0`                                     | `R0 R0 R0 R0 R0 R0 R0 R/W*`           |       `0x00` |
| `0x1C` | `0x3C` | `EIFR`       | `- INTF6 - - INTF3 INTF2 INTF1 INTF0`                     | `R0 R/W* R0 R0 R/W* R/W* R/W* R/W*`   |       `0x00` |
| `0x1D` | `0x3D` | `EIMSK`      | `- INT6 - - INT3 INT2 INT1 INT0`                          | `R0 R/W R0 R0 R/W R/W R/W R/W`        |       `0x00` |
| `0x1F` | `0x3F` | `EECR`       | `- - EEPM1 EEPM0 EERIE EEMPE EEPE EERE`                   | `R0 R0 R/W R/W R/W R/W* R/W* R/W*`    |       `0x00` |
| `0x20` | `0x40` | `EEDR`       | `EEDR[7:0]`                                               | `R/W ×8`                              |       `0x00` |
| `0x21` | `0x41` | `EEARL`      | `EEAR[7:0]`                                               | `R/W ×8`                              |       `0x00` |
| `0x22` | `0x42` | `EEARH`      | `- - - - EEAR[11:8]`                                      | `R0 R0 R0 R0 R/W R/W R/W R/W`         |       `0x00` |
| `0x23` | `0x43` | `GTCCR`      | `TSM - - - - - PSRASY PSRSYNC`                            | `R/W R0 R0 R0 R0 R0 R/W* R/W*`        |       `0x00` |
| `0x24` | `0x44` | `TCCR0A`     | `COM0A1 COM0A0 COM0B1 COM0B0 - - WGM01 WGM00`             | `R/W R/W R/W R/W R0 R0 R/W R/W`       |       `0x00` |
| `0x25` | `0x45` | `TCCR0B`     | `FOC0A FOC0B - - WGM02 CS02 CS01 CS00`                    | `W W R0 R0 R/W R/W R/W R/W`           |       `0x00` |
| `0x26` | `0x46` | `TCNT0`      | `TCNT0[7:0]`                                              | `R/W ×8`                              |       `0x00` |
| `0x27` | `0x47` | `OCR0A`      | `OCR0A[7:0]`                                              | `R/W ×8`                              |       `0x00` |
| `0x28` | `0x48` | `OCR0B`      | `OCR0B[7:0]`                                              | `R/W ×8`                              |       `0x00` |
| `0x29` | `0x49` | `PLLCSR`     | `- - - PINDIV - - PLLE PLOCK`                             | `R0 R0 R0 R/W R0 R0 R/W R`            |       `0x00` |
| `0x2C` | `0x4C` | `SPCR`       | `SPIE SPE DORD MSTR CPOL CPHA SPR1 SPR0`                  | `R/W ×8`                              |       `0x00` |
| `0x2D` | `0x4D` | `SPSR`       | `SPIF WCOL - - - - - SPI2X`                               | `R R R0 R0 R0 R0 R0 R/W`              |       `0x00` |
| `0x2E` | `0x4E` | `SPDR`       | `SPDR[7:0]`                                               | `R/W ×8`                              |       `0x00` |
| `0x30` | `0x50` | `ACSR`       | `ACD ACBG ACO ACI ACIE ACIC ACIS1 ACIS0`                  | `R/W R/W R R/W* R/W R/W R/W R/W`      | `0b00x00000` |
| `0x31` | `0x51` | `OCDR/MONDR` | `OCDR7 OCDR6 OCDR5 OCDR4 OCDR3 OCDR2 OCDR1 OCDR0`         | `R/W ×8`                              |        `N/A` |
| `0x32` | `0x52` | `PLLFRQ`     | `PINMUX PLLUSB PLLTM1 PLLTM0 PDIV3 PDIV2 PDIV1 PDIV0`     | `R/W ×8`                              |       `0x04` |
| `0x33` | `0x53` | `SMCR`       | `- - - - SM2 SM1 SM0 SE`                                  | `R0 R0 R0 R0 R/W R/W R/W R/W`         |       `0x00` |
| `0x34` | `0x54` | `MCUSR`      | `- - USBRF JTRF WDRF BORF EXTRF PORF`                     | `R0 R0 R/W* R/W* R/W* R/W* R/W* R/W*` |  reset-cause |
| `0x35` | `0x55` | `MCUCR`      | `JTD - - PUD - - IVSEL IVCE`                              | `R/W R0 R0 R/W R0 R0 R/W R/W*`        |       `0x00` |
| `0x37` | `0x57` | `SPMCSR`     | `SPMIE RWWSB SIGRD RWWSRE BLBSET PGWRT PGERS SPMEN`       | `R/W R R/W* R/W* R/W* R/W* R/W* R/W*` |       `0x00` |

For ports, the `PINx` registers are unusual: reads sample the physical synchronized pin value, and writes to `PINxn` toggle the corresponding `PORTxn` output latch. The datasheet marks `PINx` bits R/W, but their initial value is `N/A` because they reflect the pin state. ([Microchip][1])

## Extended I/O peripheral SFRs

| I/O |   Data | Name      | Bits 7 → 0                                                                | Access 7 → 0                            |          Reset |
| --: | -----: | --------- | ------------------------------------------------------------------------- | --------------------------------------- | -------------: |
|   — | `0x60` | `WDTCSR`  | `WDIF WDIE WDP3 WDCE WDE WDP2 WDP1 WDP0`                                  | `R/W* R/W R/W R/W* R/W R/W R/W R/W`     |         `0x00` |
|   — | `0x61` | `CLKPR`   | `CLKPCE - - - CLKPS3 CLKPS2 CLKPS1 CLKPS0`                                | `R/W* R0 R0 R0 R/W R/W R/W R/W`         | fuse-dependent |
|   — | `0x64` | `PRR0`    | `PRTWI - PRTIM0 - PRTIM1 PRSPI - PRADC`                                   | `R/W R0 R/W R0 R/W R/W R0 R/W`          |         `0x00` |
|   — | `0x65` | `PRR1`    | `PRUSB - - PRTIM4 PRTIM3 - - PRUSART1`                                    | `R/W R0 R0 R/W R/W R0 R0 R/W`           |         `0x00` |
|   — | `0x66` | `OSCCAL`  | `CAL7 CAL6 CAL5 CAL4 CAL3 CAL2 CAL1 CAL0`                                 | `R/W ×8`                                |    calibration |
|   — | `0x67` | `RCCTRL`  | `- - - - - - - RCFREQ`                                                    | `R0 R0 R0 R0 R0 R0 R0 R/W`              |         `0x00` |
|   — | `0x68` | `PCICR`   | `- - - - - - - PCIE0`                                                     | `R0 R0 R0 R0 R0 R0 R0 R/W`              |         `0x00` |
|   — | `0x69` | `EICRA`   | `ISC31 ISC30 ISC21 ISC20 ISC11 ISC10 ISC01 ISC00`                         | `R/W ×8`                                |         `0x00` |
|   — | `0x6A` | `EICRB`   | `- - ISC61 ISC60 - - - -`                                                 | `R0 R0 R/W R/W R0 R0 R0 R0`             |         `0x00` |
|   — | `0x6B` | `PCMSK0`  | `PCINT7 PCINT6 PCINT5 PCINT4 PCINT3 PCINT2 PCINT1 PCINT0`                 | `R/W ×8`                                |         `0x00` |
|   — | `0x6E` | `TIMSK0`  | `- - - - - OCIE0B OCIE0A TOIE0`                                           | `R0 R0 R0 R0 R0 R/W R/W R/W`            |         `0x00` |
|   — | `0x6F` | `TIMSK1`  | `- - ICIE1 - OCIE1C OCIE1B OCIE1A TOIE1`                                  | `R0 R0 R/W R0 R/W R/W R/W R/W`          |         `0x00` |
|   — | `0x71` | `TIMSK3`  | `- - ICIE3 - OCIE3C OCIE3B OCIE3A TOIE3`                                  | `R0 R0 R/W R0 R/W R/W R/W R/W`          |         `0x00` |
|   — | `0x72` | `TIMSK4`  | `OCIE4D OCIE4A OCIE4B - - TOIE4 - -`                                      | `R/W R/W R/W R0 R0 R/W R0 R0`           |         `0x00` |
|   — | `0x78` | `ADCL`    | `ADC[7:0] low/result-aligned`                                             | `R ×8`                                  |         `0x00` |
|   — | `0x79` | `ADCH`    | `ADC high/result-aligned`                                                 | `R ×8`                                  |         `0x00` |
|   — | `0x7A` | `ADCSRA`  | `ADEN ADSC ADATE ADIF ADIE ADPS2 ADPS1 ADPS0`                             | `R/W R/W* R/W R/W* R/W R/W R/W R/W`     |         `0x00` |
|   — | `0x7B` | `ADCSRB`  | `ADHSM ACME MUX5 - ADTS3 ADTS2 ADTS1 ADTS0`                               | `R R/W R/W R0 R/W R/W R/W R/W`          |         `0x00` |
|   — | `0x7C` | `ADMUX`   | `REFS1 REFS0 ADLAR MUX4 MUX3 MUX2 MUX1 MUX0`                              | `R/W ×8`                                |         `0x00` |
|   — | `0x7D` | `DIDR2`   | `- - ADC13D ADC12D ADC11D ADC10D ADC9D ADC8D`                             | `R0 R0 R/W R/W R/W R/W R/W R/W`         |         `0x00` |
|   — | `0x7E` | `DIDR0`   | `ADC7D ADC6D ADC5D ADC4D - - ADC1D ADC0D`                                 | `R/W R/W R/W R/W R0 R0 R/W R/W`         |         `0x00` |
|   — | `0x7F` | `DIDR1`   | `- - - - - - - AIN0D`                                                     | `R0 R0 R0 R0 R0 R0 R0 R/W`              |         `0x00` |
|   — | `0x80` | `TCCR1A`  | `COM1A1 COM1A0 COM1B1 COM1B0 COM1C1 COM1C0 WGM11 WGM10`                   | `R/W ×8`                                |         `0x00` |
|   — | `0x81` | `TCCR1B`  | `ICNC1 ICES1 - WGM13 WGM12 CS12 CS11 CS10`                                | `R/W R/W R0 R/W R/W R/W R/W R/W`        |         `0x00` |
|   — | `0x82` | `TCCR1C`  | `FOC1A FOC1B FOC1C - - - - -`                                             | `W W W R0 R0 R0 R0 R0`                  |         `0x00` |
|   — | `0x84` | `TCNT1L`  | `TCNT1[7:0]`                                                              | `R/W ×8`                                |         `0x00` |
|   — | `0x85` | `TCNT1H`  | `TCNT1[15:8]`                                                             | `R/W ×8`                                |         `0x00` |
|   — | `0x86` | `ICR1L`   | `ICR1[7:0]`                                                               | `R/W ×8`                                |         `0x00` |
|   — | `0x87` | `ICR1H`   | `ICR1[15:8]`                                                              | `R/W ×8`                                |         `0x00` |
|   — | `0x88` | `OCR1AL`  | `OCR1A[7:0]`                                                              | `R/W ×8`                                |         `0x00` |
|   — | `0x89` | `OCR1AH`  | `OCR1A[15:8]`                                                             | `R/W ×8`                                |         `0x00` |
|   — | `0x8A` | `OCR1BL`  | `OCR1B[7:0]`                                                              | `R/W ×8`                                |         `0x00` |
|   — | `0x8B` | `OCR1BH`  | `OCR1B[15:8]`                                                             | `R/W ×8`                                |         `0x00` |
|   — | `0x8C` | `OCR1CL`  | `OCR1C[7:0]`                                                              | `R/W ×8`                                |         `0x00` |
|   — | `0x8D` | `OCR1CH`  | `OCR1C[15:8]`                                                             | `R/W ×8`                                |         `0x00` |
|   — | `0x90` | `TCCR3A`  | `COM3A1 COM3A0 COM3B1 COM3B0 COM3C1 COM3C0 WGM31 WGM30`                   | `R/W ×8`                                |         `0x00` |
|   — | `0x91` | `TCCR3B`  | `ICNC3 ICES3 - WGM33 WGM32 CS32 CS31 CS30`                                | `R/W R/W R0 R/W R/W R/W R/W R/W`        |         `0x00` |
|   — | `0x92` | `TCCR3C`  | `FOC3A - - - - - - -`                                                     | `W R0 R0 R0 R0 R0 R0 R0`                |         `0x00` |
|   — | `0x94` | `TCNT3L`  | `TCNT3[7:0]`                                                              | `R/W ×8`                                |         `0x00` |
|   — | `0x95` | `TCNT3H`  | `TCNT3[15:8]`                                                             | `R/W ×8`                                |         `0x00` |
|   — | `0x96` | `ICR3L`   | `ICR3[7:0]`                                                               | `R/W ×8`                                |         `0x00` |
|   — | `0x97` | `ICR3H`   | `ICR3[15:8]`                                                              | `R/W ×8`                                |         `0x00` |
|   — | `0x98` | `OCR3AL`  | `OCR3A[7:0]`                                                              | `R/W ×8`                                |         `0x00` |
|   — | `0x99` | `OCR3AH`  | `OCR3A[15:8]`                                                             | `R/W ×8`                                |         `0x00` |
|   — | `0x9A` | `OCR3BL`  | `OCR3B[7:0]`                                                              | `R/W ×8`                                |         `0x00` |
|   — | `0x9B` | `OCR3BH`  | `OCR3B[15:8]`                                                             | `R/W ×8`                                |         `0x00` |
|   — | `0x9C` | `OCR3CL`  | `OCR3C[7:0]`                                                              | `R/W ×8`                                |         `0x00` |
|   — | `0x9D` | `OCR3CH`  | `OCR3C[15:8]`                                                             | `R/W ×8`                                |         `0x00` |
|   — | `0xB8` | `TWBR`    | `TWBR7 TWBR6 TWBR5 TWBR4 TWBR3 TWBR2 TWBR1 TWBR0`                         | `R/W ×8`                                |         `0x00` |
|   — | `0xB9` | `TWSR`    | `TWS7 TWS6 TWS5 TWS4 TWS3 - TWPS1 TWPS0`                                  | `R R R R R R0 R/W R/W`                  |         `0xF8` |
|   — | `0xBA` | `TWAR`    | `TWA6 TWA5 TWA4 TWA3 TWA2 TWA1 TWA0 TWGCE`                                | `R/W ×8`                                |         `0xFE` |
|   — | `0xBB` | `TWDR`    | `TWD7 TWD6 TWD5 TWD4 TWD3 TWD2 TWD1 TWD0`                                 | `R/W ×8`                                |         `0xFF` |
|   — | `0xBC` | `TWCR`    | `TWINT TWEA TWSTA TWSTO TWWC TWEN - TWIE`                                 | `R/W* R/W R/W* R/W* R R/W R0 R/W`       |         `0x00` |
|   — | `0xBD` | `TWAMR`   | `TWAM6 TWAM5 TWAM4 TWAM3 TWAM2 TWAM1 TWAM0 -`                             | `R/W R/W R/W R/W R/W R/W R/W R0`        |         `0x00` |
|   — | `0xBE` | `TCNT4`   | `TCNT4[7:0]`                                                              | `R/W ×8`                                |         `0x00` |
|   — | `0xBF` | `TC4H`    | `- - - - - TC4H2 TC4H1 TC4H0`                                             | `R0 R0 R0 R0 R0 R/W R/W R/W`            |         `0x00` |
|   — | `0xC0` | `TCCR4A`  | `COM4A1 COM4A0 COM4B1 COM4B0 FOC4A FOC4B PWM4A PWM4B`                     | `R/W R/W R/W R/W W W R/W R/W`           |         `0x00` |
|   — | `0xC1` | `TCCR4B`  | `PWM4X PSR4 DTPS41 DTPS40 CS43 CS42 CS41 CS40`                            | `R/W R/W* R/W R/W R/W R/W R/W R/W`      |         `0x00` |
|   — | `0xC2` | `TCCR4C`  | `COM4A1S COM4A0S COM4B1S COM4B0S COM4D1S COM4D0S FOC4D PWM4D`             | `R/W R/W R/W R/W R/W R/W W R/W`         |         `0x00` |
|   — | `0xC3` | `TCCR4D`  | `FPIE4 FPEN4 FPNC4 FPES4 FPAC4 FPF4 WGM41 WGM40`                          | `R/W R/W R/W R/W R/W R/W* R/W R/W`      |         `0x00` |
|   — | `0xC4` | `TCCR4E`  | `TLOCK4 ENHC4 OC4OE5 OC4OE4 OC4OE3 OC4OE2 OC4OE1 OC4OE0`                  | `R/W ×8`                                |         `0x00` |
|   — | `0xC5` | `CLKSEL0` | `RCSUT1 RCSUT0 EXSUT1 EXSUT0 RCE EXTE - CLKS`                             | `R/W R/W R/W R/W R/W R/W R0 R/W`        | fuse-dependent |
|   — | `0xC6` | `CLKSEL1` | `RCCKSEL3 RCCKSEL2 RCCKSEL1 RCCKSEL0 EXCKSEL3 EXCKSEL2 EXCKSEL1 EXCKSEL0` | `R/W ×8`                                |         `0x20` |
|   — | `0xC7` | `CLKSTA`  | `- - - - - - RCON EXTON`                                                  | `R0 R0 R0 R0 R0 R0 R R`                 |    clock-state |
|   — | `0xC8` | `UCSR1A`  | `RXC1 TXC1 UDRE1 FE1 DOR1 PE1 U2X1 MPCM1`                                 | `R R/W* R R R R R/W R/W`                |         `0x20` |
|   — | `0xC9` | `UCSR1B`  | `RXCIE1 TXCIE1 UDRIE1 RXEN1 TXEN1 UCSZ12 RXB81 TXB81`                     | `R/W ×8`                                |         `0x00` |
|   — | `0xCA` | `UCSR1C`  | `UMSEL11 UMSEL10 UPM11 UPM10 USBS1 UCSZ11 UCSZ10 UCPOL1`                  | `R/W ×8`                                |         `0x06` |
|   — | `0xCB` | `UCSR1D`  | `- - - - - - CTSEN RTSEN`                                                 | `R0 R0 R0 R0 R0 R0 R/W R/W`             |         `0x00` |
|   — | `0xCC` | `UBRR1L`  | `UBRR1[7:0]`                                                              | `R/W ×8`                                |         `0x00` |
|   — | `0xCD` | `UBRR1H`  | `- - - - UBRR1[11:8]`                                                     | `R0 R0 R0 R0 R/W R/W R/W R/W`           |         `0x00` |
|   — | `0xCE` | `UDR1`    | `UDR1[7:0]`                                                               | `R/W* ×8`                               |         `0x00` |
|   — | `0xCF` | `OCR4A`   | `OCR4A[7:0]`                                                              | `R/W ×8`                                |         `0x00` |
|   — | `0xD0` | `OCR4B`   | `OCR4B[7:0]`                                                              | `R/W ×8`                                |         `0x00` |
|   — | `0xD1` | `OCR4C`   | `OCR4C[7:0]`                                                              | `R/W ×8`                                |         `0x00` |
|   — | `0xD2` | `OCR4D`   | `OCR4D[7:0]`                                                              | `R/W ×8`                                |         `0x00` |
|   — | `0xD4` | `DT4`     | `DT4H3 DT4H2 DT4H1 DT4H0 DT4L3 DT4L2 DT4L1 DT4L0`                         | `R/W ×8`                                |         `0x00` |
|   — | `0xD7` | `UHWCON`  | `- - - - - - - UVREGE`                                                    | `R0 R0 R0 R0 R0 R0 R0 R/W`              |         `0x00` |
|   — | `0xD8` | `USBCON`  | `USBE - FRZCLK OTGPADE - - - VBUSTE`                                      | `R/W R0 R/W R/W R0 R0 R0 R/W`           |         `0x20` |
|   — | `0xD9` | `USBSTA`  | `- - - - - - ID VBUS`                                                     | `R0 R0 R0 R0 R0 R0 R R`                 |      pin-state |
|   — | `0xDA` | `USBINT`  | `- - - - - - - VBUSTI`                                                    | `R0 R0 R0 R0 R0 R0 R0 R/W*`             |         `0x00` |
|   — | `0xE0` | `UDCON`   | `- - - - RSTCPU LSM RMWKUP DETACH`                                        | `R0 R0 R0 R0 R/W R/W R/W* R/W`          |         `0x00` |
|   — | `0xE1` | `UDINT`   | `- UPRSMI EORSMI WAKEUPI EORSTI SOFI MSOFI SUSPI`                         | `R0 R/W* R/W* R/W* R/W* R/W* R/W* R/W*` |         `0x00` |
|   — | `0xE2` | `UDIEN`   | `- UPRSME EORSME WAKEUPE EORSTE SOFE MSOFE SUSPE`                         | `R0 R/W R/W R/W R/W R/W R/W R/W`        |         `0x00` |
|   — | `0xE3` | `UDADDR`  | `ADDEN UADD6 UADD5 UADD4 UADD3 UADD2 UADD1 UADD0`                         | `R/W ×8`                                |         `0x00` |
|   — | `0xE4` | `UDFNUML` | `FNUM7 FNUM6 FNUM5 FNUM4 FNUM3 FNUM2 FNUM1 FNUM0`                         | `R ×8`                                  |         `0x00` |
|   — | `0xE5` | `UDFNUMH` | `- - - - - FNUM10 FNUM9 FNUM8`                                            | `R0 R0 R0 R0 R0 R R R`                  |         `0x00` |
|   — | `0xE6` | `UDMFN`   | `- - - FNCERR - - - -`                                                    | `R0 R0 R0 R/W* R0 R0 R0 R0`             |         `0x00` |
|   — | `0xE8` | `UEINTX`  | `FIFOCON NAKINI RWAL NAKOUTI RXSTPI RXOUTI STALLEDI TXINI`                | `R/W* R/W* R R/W* R/W* R/W* R/W* R/W*`  |         `0x00` |
|   — | `0xE9` | `UENUM`   | `- - - - - EPNUM2 EPNUM1 EPNUM0`                                          | `R0 R0 R0 R0 R0 R/W R/W R/W`            |         `0x00` |
|   — | `0xEA` | `UERST`   | `- EPRST6 EPRST5 EPRST4 EPRST3 EPRST2 EPRST1 EPRST0`                      | `R0 R/W* R/W* R/W* R/W* R/W* R/W* R/W*` |         `0x00` |
|   — | `0xEB` | `UECONX`  | `- - STALLRQ STALLRQC RSTDT - - EPEN`                                     | `R0 R0 R/W* R/W* R/W* R0 R0 R/W`        |         `0x00` |
|   — | `0xEC` | `UECFG0X` | `EPTYPE1 EPTYPE0 - - - - - EPDIR`                                         | `R/W R/W R0 R0 R0 R0 R0 R/W`            |         `0x00` |
|   — | `0xED` | `UECFG1X` | `EPSIZE2 EPSIZE1 EPSIZE0 EPBK1 EPBK0 ALLOC - -`                           | `R/W R/W R/W R/W R/W R/W R0 R0`         |         `0x00` |
|   — | `0xEE` | `UESTA0X` | `CFGOK OVERFI UNDERFI - DTSEQ1 DTSEQ0 NBUSYBK1 NBUSYBK0`                  | `R R/W* R/W* R0 R R R R`                |         `0x00` |
|   — | `0xEF` | `UESTA1X` | `- - - - - CTRLDIR CURRBK1 CURRBK0`                                       | `R0 R0 R0 R0 R0 R R R`                  |         `0x00` |
|   — | `0xF0` | `UEIENX`  | `FLERRE NAKINE - NAKOUTE RXSTPE RXOUTE STALLEDE TXINE`                    | `R/W R/W R0 R/W R/W R/W R/W R/W`        |         `0x00` |
|   — | `0xF1` | `UEDATX`  | `DAT7 DAT6 DAT5 DAT4 DAT3 DAT2 DAT1 DAT0`                                 | `R/W* ×8`                               |     FIFO-state |
|   — | `0xF2` | `UEBCLX`  | `BYCT7 BYCT6 BYCT5 BYCT4 BYCT3 BYCT2 BYCT1 BYCT0`                         | `R ×8`                                  |         `0x00` |
|   — | `0xF3` | `UEBCHX`  | `- - - - - BYCT10 BYCT9 BYCT8`                                            | `R0 R0 R0 R0 R0 R R R`                  |         `0x00` |
|   — | `0xF4` | `UEINT`   | `- EPINT6 EPINT5 EPINT4 EPINT3 EPINT2 EPINT1 EPINT0`                      | `R0 R R R R R R R`                      |         `0x00` |

Important non-zero resets: `PLLFRQ = 0x04`, `CLKSEL1 = 0x20`, `TWSR = 0xF8`, `TWAR = 0xFE`, `TWDR = 0xFF`, `UCSR1A = 0x20`, `UCSR1C = 0x06`, and `USBCON = 0x20`. The TWI register reset values and access bits are explicitly listed in the TWI register section, including `TWSR = 11111000b`, `TWDR = 11111111b`, and `TWAR = 11111110b`. ([Microchip][1])

For cycle-accurate emulation, do not treat every `R/W*` bit as ordinary storage. Timer interrupt flags, external interrupt flags, ADC `ADIF`, analog comparator `ACI`, USART `TXC1`, TWI `TWINT`, EEPROM programming strobes, watchdog change-enable bits, PLL lock, USB interrupt/status bits, and endpoint FIFO bits all have peripheral-specific set/clear timing and side effects. The datasheet also warns that some status flags are cleared by writing a logical one, and that `SBI/CBI` on low I/O registers can accidentally clear set flags. ([Microchip][1])

[1]: https://ww1.microchip.com/downloads/en/devicedoc/atmel-7766-8-bit-avr-atmega16u4-32u4_datasheet.pdf "ATmega16U4/32U4 Datasheet"
