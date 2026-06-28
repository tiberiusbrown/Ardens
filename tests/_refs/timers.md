Below is a practical reference to the **ATmega32U4 timer/counter hardware**: **Timer0**, **Timer1**, **Timer3**, and **Timer4**. The 32U4 does **not** have Timer2. Timer4 is the unusual one: it is a high-speed 10-bit timer that can run from the PLL clock and has dead-time, complementary PWM, enhanced PWM, and fault protection.

## 1. Timer inventory

| Timer              |             Width | Main purpose                               |                                    Compare channels | Special features                                                                  |
| ------------------ | ----------------: | ------------------------------------------ | --------------------------------------------------: | --------------------------------------------------------------------------------- |
| **Timer/Counter0** |             8-bit | General timing, compare interrupts, PWM    |                                                A, B | CTC, Fast PWM, Phase Correct PWM                                                  |
| **Timer/Counter1** |            16-bit | Precision timing, PWM, input capture       |                                             A, B, C | Input capture, external event counter, 16-bit PWM                                 |
| **Timer/Counter3** |            16-bit | Similar to Timer1                          |                                             A, B, C | Input capture, 16-bit PWM; no external T3 clock input on this product             |
| **Timer/Counter4** | 10-bit high-speed | High-speed PWM and motor/control waveforms | A, B, C, D, but A/B/D are output channels; C is TOP | PLL/high-speed clocking, dead-time, complementary outputs, PWM6, fault protection |

Timer0 has two compare units and three interrupt sources; Timer1/3 are 16-bit timers with three compare units, one input-capture unit, and ten total interrupt sources across the two timers; Timer4 is a high-speed timer with up to 10-bit accuracy, three main PWM output channels, dead-time generation, synchronous update, and five interrupt sources. ([Microchip][1])

## 2. Shared timer concepts

A timer/counter is basically a counter register plus control logic. The counter increments, decrements, clears, or wraps depending on the mode. Compare registers are continuously compared against the counter value. On a match, hardware can set an interrupt flag, toggle/set/clear an output pin, or participate in PWM generation.

The common terms are:

| Term         | Meaning                                                                                                                         |
| ------------ | ------------------------------------------------------------------------------------------------------------------------------- |
| **BOTTOM**   | Counter value 0                                                                                                                 |
| **MAX**      | Largest value representable by the counter: 0xFF for Timer0, 0xFFFF for Timer1/3, 0x3FF for Timer4 in 10-bit mode               |
| **TOP**      | The highest value in the active counting sequence. It may be fixed, or defined by OCRnA, ICRn, or OCR4C depending on timer/mode |
| **OCRnx**    | Output Compare Register for timer `n`, channel `x`                                                                              |
| **TCNTn**    | Timer/Counter register                                                                                                          |
| **TOVn**     | Timer overflow flag                                                                                                             |
| **OCFnx**    | Output compare-match flag                                                                                                       |
| **COMnx1:0** | Compare-output mode bits                                                                                                        |
| **WGM bits** | Waveform Generation Mode bits                                                                                                   |
| **CS bits**  | Clock Select / prescaler bits                                                                                                   |

Timer0, Timer1, and Timer3 share a synchronous prescaler module; resetting that prescaler affects all three. The `GTCCR` register provides timer synchronization mode and prescaler reset bits. ([Microchip][1])

## 3. Power and clock gating registers

Before using a timer, make sure it is not disabled in the power-reduction registers.

| Register  | Relevant bits              |
| --------- | -------------------------- |
| **PRR0**  | `PRTIM0`, `PRTIM1`         |
| **PRR1**  | `PRTIM3`, `PRTIM4`         |
| **GTCCR** | `TSM`, `PSRASY`, `PSRSYNC` |

Writing a one to `PRTIM0`, `PRTIM1`, `PRTIM3`, or `PRTIM4` shuts down that timer module; clearing the bit enables the timer again. `GTCCR.PSRSYNC` resets the shared prescaler for Timer0/1/3, and `GTCCR.TSM` can hold timers halted during synchronized setup. ([Microchip][1])

---

# Timer/Counter0: 8-bit timer with PWM

Timer0 is an 8-bit general-purpose timer with two output-compare channels: **A** and **B**. It can be clocked internally through the prescaler or externally from the **T0** pin. Its compare registers `OCR0A` and `OCR0B` are compared continuously against `TCNT0`; compare matches set `OCF0A` or `OCF0B`, and can drive output waveform generation. ([Microchip][1])

## Timer0 main registers

| Register   | Bits / purpose                                     |
| ---------- | -------------------------------------------------- |
| **TCCR0A** | `COM0A1:0`, `COM0B1:0`, `WGM01:0`                  |
| **TCCR0B** | `FOC0A`, `FOC0B`, `WGM02`, `CS02:0`                |
| **TCNT0**  | 8-bit counter value                                |
| **OCR0A**  | Compare value A; can also define TOP in some modes |
| **OCR0B**  | Compare value B                                    |
| **TIMSK0** | Interrupt enables: `OCIE0B`, `OCIE0A`, `TOIE0`     |
| **TIFR0**  | Interrupt flags: `OCF0B`, `OCF0A`, `TOV0`          |

`TCCR0A` holds the compare-output controls and the low two waveform-generation bits. `TCCR0B` holds the force-compare strobes, the high waveform-generation bit, and the clock-select bits. `TIMSK0` enables compare/overflow interrupts; `TIFR0` reports and clears compare/overflow flags. Flags are cleared either by executing the corresponding ISR or by writing a logic one to the flag bit. ([Microchip][1])

## Timer0 clock select

`TCCR0B.CS02:0` selects the clock:

| CS02:0 | Clock source              |
| ------ | ------------------------- |
| `000`  | Stopped                   |
| `001`  | `clkI/O` / 1              |
| `010`  | `clkI/O` / 8              |
| `011`  | `clkI/O` / 64             |
| `100`  | `clkI/O` / 256            |
| `101`  | `clkI/O` / 1024           |
| `110`  | External T0, falling edge |
| `111`  | External T0, rising edge  |

External T0 transitions can clock Timer0 even if the pin is configured as an output, which allows software-controlled counting. ([Microchip][1])

## Timer0 modes

Timer0 uses `WGM02:0`, split between `TCCR0B.WGM02` and `TCCR0A.WGM01:0`.

| Mode | WGM02:0 | Mode              | TOP   | OCR update | TOV0 set |
| ---: | ------- | ----------------- | ----- | ---------- | -------- |
|    0 | `000`   | Normal            | 0xFF  | Immediate  | MAX      |
|    1 | `001`   | Phase Correct PWM | 0xFF  | TOP        | BOTTOM   |
|    2 | `010`   | CTC               | OCR0A | Immediate  | MAX      |
|    3 | `011`   | Fast PWM          | 0xFF  | TOP        | MAX      |
|    4 | `100`   | Reserved          | —     | —          | —        |
|    5 | `101`   | Phase Correct PWM | OCR0A | TOP        | BOTTOM   |
|    6 | `110`   | Reserved          | —     | —          | —        |
|    7 | `111`   | Fast PWM          | OCR0A | TOP        | TOP      |

`OCR0A` can act as TOP in Timer0 CTC mode and in the variable-period PWM modes. In PWM modes, `OCR0A`/`OCR0B` are double-buffered so updates occur at defined points in the waveform, reducing glitches. ([Microchip][1])

## Timer0 compare output behavior

In non-PWM modes, `COM0x1:0` selects disconnected/toggle/clear/set on compare match. In Fast PWM, the usual non-inverting setting is `COM0x1:0 = 10`, meaning clear on compare match and set at TOP; inverted PWM is `11`. In Phase Correct PWM, non-inverting clears on compare while up-counting and sets on compare while down-counting; inverted does the opposite. ([Microchip][1])

A useful gotcha: writing `TCNT0` blocks compare matches for the following timer clock, so changing the counter while compare output is active can miss a match. ([Microchip][1])

---

# Timer/Counter1 and Timer/Counter3: 16-bit timers

Timer1 and Timer3 are very similar. They are true 16-bit timers with three output-compare channels each, plus an input-capture unit. They support Normal, CTC, Fast PWM, Phase Correct PWM, and Phase and Frequency Correct PWM modes. Timer1/3 can be clocked internally through the shared prescaler; Timer1 can also use the external **T1** clock input, while the datasheet notes that the external `Tn` clock input only refers to T1 because T3 input is not available on this product. ([Microchip][1])

## Timer1/3 main registers

For Timer1, replace `n` with `1`; for Timer3, replace `n` with `3`.

| Register          | Purpose                                                                                                |
| ----------------- | ------------------------------------------------------------------------------------------------------ |
| **TCCRnA**        | `COMnA1:0`, `COMnB1:0`, `COMnC1:0`, `WGMn1:0`                                                          |
| **TCCRnB**        | `ICNCn`, `ICESn`, `WGMn3:2`, `CSn2:0`                                                                  |
| **TCCRnC**        | Force-output-compare strobes, especially `FOCnA/B/C` for Timer1; Timer3 exposes `FOC3A` in this device |
| **TCNTnH:TCNTnL** | 16-bit counter                                                                                         |
| **OCRnAH:L**      | Compare A                                                                                              |
| **OCRnBH:L**      | Compare B                                                                                              |
| **OCRnCH:L**      | Compare C                                                                                              |
| **ICRnH:L**       | Input Capture Register; can also be TOP in selected modes                                              |
| **TIMSKn**        | Interrupt enables: `ICIEn`, `OCIEnC`, `OCIEnB`, `OCIEnA`, `TOIEn`                                      |
| **TIFRn**         | Flags: `ICFn`, `OCFnC`, `OCFnB`, `OCFnA`, `TOVn`                                                       |

The 16-bit registers use a temporary high-byte register internally, so accesses must follow the AVR 16-bit access rules. In C, the compiler generally handles this, but if both main code and ISRs access 16-bit timer registers, protect the access with interrupt disable/restore so the shared temporary register is not corrupted. ([Microchip][1])

## Timer1/3 clock select

`TCCRnB.CSn2:0` selects the clock:

| CSn2:0 | Clock source              |
| ------ | ------------------------- |
| `000`  | Stopped                   |
| `001`  | `clkI/O` / 1              |
| `010`  | `clkI/O` / 8              |
| `011`  | `clkI/O` / 64             |
| `100`  | `clkI/O` / 256            |
| `101`  | `clkI/O` / 1024           |
| `110`  | External Tn, falling edge |
| `111`  | External Tn, rising edge  |

The external-clock modes are meaningful for T1 on this device; T3 does not have an external T3 input. ([Microchip][1])

## Timer1/3 modes

Timer1/3 use `WGMn3:0`, split between `TCCRnB.WGMn3:2` and `TCCRnA.WGMn1:0`.

| Mode | WGMn3:0 | Mode                          | TOP    | OCR update | TOVn set |
| ---: | ------- | ----------------------------- | ------ | ---------- | -------- |
|    0 | `0000`  | Normal                        | 0xFFFF | Immediate  | MAX      |
|    1 | `0001`  | Phase Correct PWM, 8-bit      | 0x00FF | TOP        | BOTTOM   |
|    2 | `0010`  | Phase Correct PWM, 9-bit      | 0x01FF | TOP        | BOTTOM   |
|    3 | `0011`  | Phase Correct PWM, 10-bit     | 0x03FF | TOP        | BOTTOM   |
|    4 | `0100`  | CTC                           | OCRnA  | Immediate  | MAX      |
|    5 | `0101`  | Fast PWM, 8-bit               | 0x00FF | TOP        | TOP      |
|    6 | `0110`  | Fast PWM, 9-bit               | 0x01FF | TOP        | TOP      |
|    7 | `0111`  | Fast PWM, 10-bit              | 0x03FF | TOP        | TOP      |
|    8 | `1000`  | Phase & Frequency Correct PWM | ICRn   | BOTTOM     | BOTTOM   |
|    9 | `1001`  | Phase & Frequency Correct PWM | OCRnA  | BOTTOM     | BOTTOM   |
|   10 | `1010`  | Phase Correct PWM             | ICRn   | TOP        | BOTTOM   |
|   11 | `1011`  | Phase Correct PWM             | OCRnA  | TOP        | BOTTOM   |
|   12 | `1100`  | CTC                           | ICRn   | Immediate  | MAX      |
|   13 | `1101`  | Reserved                      | —      | —          | —        |
|   14 | `1110`  | Fast PWM                      | ICRn   | TOP        | TOP      |
|   15 | `1111`  | Fast PWM                      | OCRnA  | TOP        | TOP      |

The big distinction is where TOP comes from. Fixed-resolution PWM uses 0x00FF/0x01FF/0x03FF. Variable-period PWM uses `ICRn` or `OCRnA` as TOP. `OCRnA` is double-buffered in PWM modes, which can make it safer than `ICRn` when the PWM frequency is changed dynamically. ([Microchip][1])

## Timer1/3 operating modes explained

**Normal mode** counts upward from 0x0000 to 0xFFFF and wraps. `TOVn` is set when the counter wraps to zero. This is good for free-running timebases, long software timers, and input capture measurement. ([Microchip][1])

**CTC mode** clears the counter when it reaches TOP, where TOP is either `OCRnA` or `ICRn`. This is ideal for periodic interrupts or square-wave generation. The datasheet warns that CTC TOP updates are not double-buffered; if you write a TOP value lower than the current `TCNTn`, the compare can be missed until the counter wraps. ([Microchip][1])

**Fast PWM** is single-slope PWM: the counter counts BOTTOM to TOP, then clears. It gives the highest PWM frequency for a given TOP. Non-inverting mode clears the output on compare and sets it at TOP/BOTTOM depending on the exact mode. ([Microchip][1])

**Phase Correct PWM** is dual-slope: the counter counts up to TOP and then back down. It has lower frequency than Fast PWM but gives center-aligned PWM.

**Phase and Frequency Correct PWM** is also dual-slope, but OCR updates happen at BOTTOM. That gives symmetrical periods when TOP changes, which is useful when you want cleaner frequency changes. ([Microchip][1])

## Input capture on Timer1/3

The input-capture unit copies the current `TCNTn` value into `ICRn` on a selected edge of the input-capture pin. `TCCRnB.ICESn` selects falling or rising edge, and `TCCRnB.ICNCn` enables a noise canceler that requires four equal samples before accepting an edge, adding a four-cycle delay. The input-capture flag `ICFn` can generate an interrupt through `TIMSKn.ICIEn`. `ICRn` can also be used as TOP in some PWM/CTC modes; when it is used as TOP, the input-capture pin function is disconnected. ([Microchip][1])

---

# Timer/Counter4: high-speed 10-bit timer

Timer4 is the 32U4’s special high-speed timer. It can run up to 64 MHz, supports 10-bit operation using an extra high-byte register `TC4H`, and can alternatively be used as an 8-bit timer if the upper bits are zero. It has output compare registers `OCR4A`, `OCR4B`, `OCR4C`, and `OCR4D`; `OCR4C` is especially important because it defines TOP and therefore the period. ([Microchip][1])

## Timer4 main registers

| Register   | Purpose                                                                         |
| ---------- | ------------------------------------------------------------------------------- |
| **TCCR4A** | `COM4A1:0`, `COM4B1:0`, `FOC4A`, `FOC4B`, `PWM4A`, `PWM4B`                      |
| **TCCR4B** | `PWM4X`, `PSR4`, `DTPS41:40`, `CS43:0`                                          |
| **TCCR4C** | shadow compare bits for A/B, `COM4D1:0`, `FOC4D`, `PWM4D`                       |
| **TCCR4D** | `FPIE4`, `FPEN4`, `FPNC4`, `FPES4`, `FPAC4`, `FPF4`, `WGM41:0`                  |
| **TCCR4E** | `TLOCK4`, `ENHC4`, `OC4OE5:0`                                                   |
| **TCNT4**  | Low 8 bits of counter                                                           |
| **TC4H**   | Extra high bits for 10-bit access; also supports enhanced 11-bit compare access |
| **OCR4A**  | Compare A                                                                       |
| **OCR4B**  | Compare B                                                                       |
| **OCR4C**  | Compare C; defines TOP and clears `TCNT4` on match                              |
| **OCR4D**  | Compare D                                                                       |
| **TIMSK4** | Interrupt enables: `OCIE4D`, `OCIE4A`, `OCIE4B`, `TOIE4`                        |
| **TIFR4**  | Flags: `OCF4D`, `OCF4A`, `OCF4B`, `TOV4`                                        |
| **DT4**    | Dead-time value: `DT4H3:0`, `DT4L3:0`                                           |

`OCR4C` is the period/TOP register. A compare match with `OCR4C` clears `TCNT4`, and the datasheet notes that values below three are automatically replaced by three. ([Microchip][1])

## Timer4 10-bit access

Timer4’s visible counter/compare registers are 8-bit registers, but the timer logic can be 10-bit. To access 10-bit values, write the high bits to `TC4H` first, then write the low byte to `TCNT4` or `OCR4x`. For reads, read the low byte first, then read `TC4H`. Because `TC4H` is shared across Timer4’s 10-bit registers, protect multi-byte accesses if an ISR also touches Timer4. ([Microchip][1])

## Timer4 clock select

Timer4 has its own prescaler and can use either the synchronous system clock `CK` or the asynchronous PLL clock `PCK`, depending on Timer4/PLL configuration. `TCCR4B.CS43:0` selects:

| CS43:0 | Timer4 clock |
| ------ | ------------ |
| `0000` | Stopped      |
| `0001` | /1           |
| `0010` | /2           |
| `0011` | /4           |
| `0100` | /8           |
| `0101` | /16          |
| `0110` | /32          |
| `0111` | /64          |
| `1000` | /128         |
| `1001` | /256         |
| `1010` | /512         |
| `1011` | /1024        |
| `1100` | /2048        |
| `1101` | /4096        |
| `1110` | /8192        |
| `1111` | /16384       |

The same divisor table applies whether Timer4 is using asynchronous `PCK` or synchronous `CK`; only the source clock changes. ([Microchip][1])

## Timer4 modes

Timer4 mode selection is different from the other timers. It uses the per-channel `PWM4x` bits plus `TCCR4D.WGM41:0`.

| PWM4x | WGM41:0 | Mode                          | TOP   | OCR update | TOV4 set |
| ----- | ------- | ----------------------------- | ----- | ---------- | -------- |
| 0     | `xx`    | Normal                        | OCR4C | Immediate  | TOP      |
| 1     | `00`    | Fast PWM                      | OCR4C | TOP        | TOP      |
| 1     | `01`    | Phase & Frequency Correct PWM | OCR4C | BOTTOM     | BOTTOM   |
| 1     | `10`    | PWM6 single-slope             | OCR4C | TOP        | TOP      |
| 1     | `11`    | PWM6 dual-slope               | OCR4C | BOTTOM     | BOTTOM   |

Timer4’s Normal mode still counts from BOTTOM to TOP, where TOP is `OCR4C`, then restarts from BOTTOM. Fast PWM is single-slope; Phase and Frequency Correct PWM is dual-slope; PWM6 is intended for six-output motor-control-style waveforms. ([Microchip][1])

## Timer4 dead-time and complementary outputs

Timer4 can generate complementary PWM output pairs with programmable dead time. `DT4` holds two four-bit dead-time values: `DT4H` and `DT4L`. `TCCR4B.DTPS41:40` selects a dead-time prescaler of /1, /2, /4, or /8. This lets you delay complementary output transitions so both sides of a half-bridge are not on at the same time. ([Microchip][1])

## Timer4 enhanced PWM

`TCCR4E.ENHC4` enables Enhanced Compare/PWM mode. In this mode, `OCR4A`, `OCR4B`, and `OCR4D` effectively get one extra resolution bit without changing the PWM frequency. `OCR4C` still defines the period and does not include the extra enhanced bit. ([Microchip][1])

## Timer4 synchronous update lock

`TCCR4E.TLOCK4` lets software write PWM registers without immediately affecting the outputs. Values are stored and then applied when `TLOCK4` is cleared. This is useful when updating multiple PWM channels together so they change coherently. ([Microchip][1])

## Timer4 fault protection

Timer4 has a fault-protection unit. A fault can be triggered from **INT0** or from the analog comparator, with selectable edge and optional noise canceling. When a fault is triggered, Timer4 clears the compare-output mode bits, disconnects the output comparators from the PWM pins, clears `FPEN4`, and can raise the fault-protection interrupt flag `FPF4`. ([Microchip][1])

---

# Practical mode selection guide

| Need                                        | Use                                      |
| ------------------------------------------- | ---------------------------------------- |
| Simple periodic interrupt                   | Timer0/1/3 CTC                           |
| Long free-running timestamp                 | Timer1 or Timer3 Normal mode             |
| Measure pulse width/frequency               | Timer1/3 input capture                   |
| Simple low/medium PWM                       | Timer0, Timer1, or Timer3 Fast PWM       |
| Center-aligned PWM                          | Timer1/3 Phase Correct PWM               |
| Frequency changes with symmetric PWM        | Timer1/3 Phase and Frequency Correct PWM |
| High-frequency PWM                          | Timer4, often from PLL/PCK               |
| Complementary PWM with dead-time            | Timer4                                   |
| Six-output motor-control style PWM          | Timer4 PWM6                              |
| Synchronized update of multiple PWM outputs | Timer4 with `TLOCK4`                     |

The most important conceptual split is this: **Timer0/1/3 are conventional AVR timers**, while **Timer4 is a specialized high-speed PWM/timing block**. For ordinary timekeeping or compare interrupts, Timer1/3 are the cleanest. For high-speed PWM, complementary outputs, or dead-time insertion, Timer4 is the distinctive 32U4 peripheral.

[1]: https://ww1.microchip.com/downloads/en/devicedoc/atmel-7766-8-bit-avr-atmega16u4-32u4_datasheet.pdf "ATmega16U4/32U4 Datasheet"
