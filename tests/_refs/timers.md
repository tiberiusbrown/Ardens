Below is the revised reference, written as an **implementation-oriented model** for a cycle-accurate ATmega32U4 timer emulator.

I’m treating “cycle-accurate” as: CPU-visible timer registers, interrupt flags, interrupt requests, counter values, compare behavior, and output-compare pin state are correct at **CPU-cycle resolution**. For **Timer4**, which can run from the 64 MHz PLL while the CPU runs at 16 MHz, you should model Timer4 using sub-cycles or a fractional clock accumulator.

# ATmega32U4 timer/counter implementation reference

The ATmega32U4 contains these timer/counter peripherals:

| Peripheral         |             Width | Main clock domain               | Channels   | Major features                                                               |
| ------------------ | ----------------: | ------------------------------- | ---------- | ---------------------------------------------------------------------------- |
| **Timer/Counter0** |             8-bit | `clkI/O`, shared sync prescaler | A, B       | Normal, CTC, Fast PWM, Phase Correct PWM                                     |
| **Timer/Counter1** |            16-bit | `clkI/O`, shared sync prescaler | A, B, C    | Input capture, CTC, Fast PWM, Phase Correct PWM, Phase/Frequency Correct PWM |
| **Timer/Counter3** |            16-bit | `clkI/O`, shared sync prescaler | A, B, C    | Similar to Timer1; external T3 clock input is not present on this device     |
| **Timer/Counter4** | 10-bit high-speed | `CK` or PLL-derived `PCK`       | A, B, C, D | High-speed PWM, OCR4C TOP, dead-time, complementary PWM, fault protection    |

There is **no Timer2** on the ATmega32U4; the datasheet explicitly notes that asynchronous Timer2 behavior is not present on this device family. ([Microchip][1])

---

# 1. Global emulation model

## 1.1 Suggested per-CPU-cycle update order

For an emulator that advances one CPU cycle at a time, use this high-level order:

```text
for each CPU cycle:
    1. Execute CPU micro-operation / instruction-cycle effects.
       - Apply I/O reads/writes occurring in this CPU cycle.
       - Handle W1C flag clears and force-compare strobes.

    2. Sample external timer pins on clkI/O boundary.
       - T0 / T1 external clock inputs.
       - ICP1 / ICP3 input-capture pins.
       - Timer4 fault source if using external INT0 path.

    3. Advance synchronous shared prescaler for Timer0/1/3.
       - Unless halted by GTCCR.TSM.
       - Unless reset/held by GTCCR.PSRSYNC.
       - Produce timer ticks for timers whose CS bits select an internal prescaled clock.

    4. Advance Timer0, Timer1, Timer3 for each generated timer tick.

    5. Advance Timer4.
       - If Timer4 uses CK, one possible timer-source edge per CPU cycle.
       - If Timer4 uses PCK, run zero or more Timer4 sub-ticks per CPU cycle.

    6. Latch/set interrupt flags.
       - Flags are set whether or not the corresponding interrupt enable bit is set.

    7. Present interrupt requests to the CPU interrupt controller.
       - Interrupt request = flag set && interrupt enable && SREG.I.
```

The exact order of CPU writes versus timer ticks matters. The AVR datasheet states that CPU writes to timer counter registers override counter clear/count operations, and writes to `TCNTn` can block compare matches for the following timer clock. Your emulator should therefore give CPU counter writes priority over timer-generated count/clear activity in the same modeled CPU cycle. ([Microchip][1])

## 1.2 Clock domains

Use at least these clock domains:

```text
CPU clock / clkI/O:
    - CPU execution
    - I/O register access
    - Timer0/1/3 synchronous clocking
    - T0/T1/ICP sampling

Timer4 source clock:
    - CK, synchronous with CPU clock
    - or PCK, PLL-derived, potentially faster than CPU clock
```

On a typical Arduino Leonardo / Arduboy-style 32U4 system, the CPU often runs at **16 MHz** and Timer4 may be clocked from a **64 MHz PLL-derived clock**, so Timer4 can receive **four source-clock opportunities per CPU cycle** before its own Timer4 prescaler is applied. The datasheet describes Timer4 as a high-speed timer that can run from the asynchronous peripheral clock `PCK` and notes synchronization behavior between the CPU clock domain and Timer4’s clock domain. ([Microchip][1])

A robust implementation should therefore represent Timer4 time as:

```text
timer4_phase += timer4_source_hz / cpu_hz

while timer4_phase >= 1:
    timer4_phase -= 1
    timer4_source_tick()
```

Then apply the Timer4 prescaler to those source ticks.

---

# 2. Interrupt flags, masks, and vectors

Timer interrupt flags are set by hardware events regardless of whether their interrupt-enable bits are set. Interrupt-enable bits merely control whether a set flag requests an interrupt.

Most timer flags are cleared by writing a logic one to the flag bit. This means:

```text
TIFR0 |= OCF0A      // hardware sets flag
write TIFR0, OCF0A // software clears flag
write TIFR0, 0     // no effect
```

Be careful with AVR bit instructions such as `SBI` on flag registers. The datasheet warns that writing one clears interrupt flags, so read-modify-write bit operations on `TIFR` registers can accidentally clear flags that became set between the read and the write. ([Microchip][1])

## 2.1 Timer interrupt vectors

The relevant ATmega32U4 timer interrupt vectors are:

| Vector address | Source         |
| -------------: | -------------- |
|       `0x0020` | `TIMER1_CAPT`  |
|       `0x0022` | `TIMER1_COMPA` |
|       `0x0024` | `TIMER1_COMPB` |
|       `0x0026` | `TIMER1_COMPC` |
|       `0x0028` | `TIMER1_OVF`   |
|       `0x002A` | `TIMER0_COMPA` |
|       `0x002C` | `TIMER0_COMPB` |
|       `0x002E` | `TIMER0_OVF`   |
|       `0x003E` | `TIMER3_CAPT`  |
|       `0x0040` | `TIMER3_COMPA` |
|       `0x0042` | `TIMER3_COMPB` |
|       `0x0044` | `TIMER3_COMPC` |
|       `0x0046` | `TIMER3_OVF`   |
|       `0x004C` | `TIMER4_COMPA` |
|       `0x004E` | `TIMER4_COMPB` |
|       `0x0050` | `TIMER4_COMPD` |
|       `0x0052` | `TIMER4_OVF`   |
|       `0x0054` | `TIMER4_FPF`   |

The vector table is shifted if interrupt vectors are moved to the boot section with `IVSEL`, but the relative ordering is the same. ([Microchip][1])

---

# 3. Register map and I/O behavior

AVR has both an **I/O address space** and a **data memory address space**. For low I/O registers, the data-memory address is generally `I/O address + 0x20`. Extended I/O registers are only accessible with load/store-style instructions such as `LDS`, `STS`, `LD`, and `ST`, not with `IN`/`OUT`. Reserved bits should be read as zero and written as zero in a clean implementation. ([Microchip][1])

## 3.1 Shared / power registers

| Register | Data address | Purpose                                 |
| -------- | -----------: | --------------------------------------- |
| `GTCCR`  |       `0x43` | General Timer/Counter Control Register  |
| `PRR0`   |       `0x64` | Power reduction for Timer0/1 and others |
| `PRR1`   |       `0x65` | Power reduction for Timer3/4 and others |

Relevant bits:

| Register | Bits                       |
| -------- | -------------------------- |
| `GTCCR`  | `TSM`, `PSRASY`, `PSRSYNC` |
| `PRR0`   | `PRTIM0`, `PRTIM1`         |
| `PRR1`   | `PRTIM3`, `PRTIM4`         |

When a timer’s power-reduction bit is set, the timer clock is stopped, the module state is frozen, and its I/O registers are not accessible in the normal way. Clearing the power-reduction bit resumes the timer from its previous state. ([Microchip][1])

## 3.2 Timer0 registers

| Register | Data address | I/O address | Notes                           |
| -------- | -----------: | ----------: | ------------------------------- |
| `TIFR0`  |       `0x35` |      `0x15` | Flags, W1C                      |
| `TCCR0A` |       `0x44` |      `0x24` | COM and WGM low bits            |
| `TCCR0B` |       `0x45` |      `0x25` | FOC, WGM high bit, clock select |
| `TCNT0`  |       `0x46` |      `0x26` | Counter                         |
| `OCR0A`  |       `0x47` |      `0x27` | Compare A / optional TOP        |
| `OCR0B`  |       `0x48` |      `0x28` | Compare B                       |
| `TIMSK0` |       `0x6E` |    extended | Interrupt mask                  |

Timer0 has two output-compare units, A and B, and supports compare-match flags, overflow flags, compare interrupts, overflow interrupts, and waveform generation on the output-compare pins. ([Microchip][1])

## 3.3 Timer1 registers

| Register   | Data address | Notes                                   |
| ---------- | -----------: | --------------------------------------- |
| `TIFR1`    |       `0x36` | Flags, W1C                              |
| `TIMSK1`   |       `0x6F` | Interrupt mask                          |
| `TCCR1A`   |       `0x80` | COM A/B/C, WGM low bits                 |
| `TCCR1B`   |       `0x81` | ICNC, ICES, WGM high bits, clock select |
| `TCCR1C`   |       `0x82` | Force compare                           |
| `TCNT1L/H` |  `0x84/0x85` | 16-bit counter                          |
| `ICR1L/H`  |  `0x86/0x87` | Input capture / optional TOP            |
| `OCR1AL/H` |  `0x88/0x89` | Compare A / optional TOP                |
| `OCR1BL/H` |  `0x8A/0x8B` | Compare B                               |
| `OCR1CL/H` |  `0x8C/0x8D` | Compare C                               |

## 3.4 Timer3 registers

| Register   | Data address | Notes                                   |
| ---------- | -----------: | --------------------------------------- |
| `TIFR3`    |       `0x38` | Flags, W1C                              |
| `TIMSK3`   |       `0x71` | Interrupt mask                          |
| `TCCR3A`   |       `0x90` | COM A/B/C, WGM low bits                 |
| `TCCR3B`   |       `0x91` | ICNC, ICES, WGM high bits, clock select |
| `TCCR3C`   |       `0x92` | Force compare                           |
| `TCNT3L/H` |  `0x94/0x95` | 16-bit counter                          |
| `ICR3L/H`  |  `0x96/0x97` | Input capture / optional TOP            |
| `OCR3AL/H` |  `0x98/0x99` | Compare A / optional TOP                |
| `OCR3BL/H` |  `0x9A/0x9B` | Compare B                               |
| `OCR3CL/H` |  `0x9C/0x9D` | Compare C                               |

Timer1 and Timer3 are very similar 16-bit timers with input capture, output compare units A/B/C, multiple PWM modes, and shared-style register behavior. ([Microchip][1])

## 3.5 Timer4 registers

| Register | Data address | Notes                                          |
| -------- | -----------: | ---------------------------------------------- |
| `TIFR4`  |       `0x39` | Timer4 flags, W1C                              |
| `TIMSK4` |       `0x72` | Timer4 interrupt mask                          |
| `TCNT4`  |       `0xBE` | Low byte of Timer4 counter                     |
| `TC4H`   |       `0xBF` | Shared high bits for 10-bit Timer4 accesses    |
| `TCCR4A` |       `0xC0` | COM4A/B, FOC4A/B, PWM4A/B                      |
| `TCCR4B` |       `0xC1` | PWM4X, PSR4, dead-time prescaler, clock select |
| `TCCR4C` |       `0xC2` | Shadow COM bits, COM4D, FOC4D, PWM4D           |
| `TCCR4D` |       `0xC3` | Fault protection, WGM bits                     |
| `TCCR4E` |       `0xC4` | TLOCK4, ENHC4, OC4OE bits                      |
| `OCR4A`  |       `0xCF` | Compare A                                      |
| `OCR4B`  |       `0xD0` | Compare B                                      |
| `OCR4C`  |       `0xD1` | Compare C / TOP                                |
| `OCR4D`  |       `0xD2` | Compare D                                      |
| `DT4`    |       `0xD4` | Dead-time values                               |

Timer4 is register-mapped in extended I/O/data space and includes special synchronization, high-byte, dead-time, fault, and enhanced-PWM behavior not found in Timer0/1/3. ([Microchip][1])

---

# 4. Shared Timer0/1/3 prescaler

Timer0, Timer1, and Timer3 use the synchronous timer prescaler structure. The external clock logic samples `T0`/`T1` on the system clock, and the datasheet gives an external edge delay of roughly 2.5 to 3.5 system-clock cycles due to synchronization and edge detection. It also says each half-period of the external signal must be longer than one system-clock cycle and recommends keeping the external frequency below `f_clk_I/O / 2.5`; the external T3 input is not available on this device. ([Microchip][1])

## 4.1 Internal prescaler model

Use one shared synchronous prescaler phase for Timer0/1/3:

```text
shared_prescaler_counter
    increments once per clkI/O cycle
    reset by GTCCR.PSRSYNC
    halted while GTCCR.TSM = 1
```

Each timer independently selects a tap through its `CSn2:0` bits.

| `CSn2:0` | Timer0/1/3 clock              |
| -------- | ----------------------------- |
| `000`    | stopped                       |
| `001`    | `clkI/O / 1`                  |
| `010`    | `clkI/O / 8`                  |
| `011`    | `clkI/O / 64`                 |
| `100`    | `clkI/O / 256`                |
| `101`    | `clkI/O / 1024`               |
| `110`    | external Tn pin, falling edge |
| `111`    | external Tn pin, rising edge  |

For Timer0 this external pin is `T0`. For Timer1 it is `T1`. For Timer3, do not implement a real external T3 pin on the 32U4 because the datasheet notes that the T3 input is not available on this product. ([Microchip][1])

## 4.2 `GTCCR`

`GTCCR.TSM` is the Timer/Counter Synchronization Mode bit. When set, it keeps the prescaler reset signals asserted, effectively halting the affected timers so software can configure multiple timers synchronously. When `TSM` is cleared, the reset signals are released and the timers begin counting from a synchronized prescaler state. `GTCCR.PSRSYNC` resets the synchronous prescaler shared by Timer0, Timer1, and Timer3. ([Microchip][1])

Recommended implementation:

```c
if (GTCCR.TSM) {
    hold_timer0_1_3_prescaler_reset = true;
    do_not_advance_timer0_1_3_from_internal_clocks();
}

if (write_to_GTCCR_sets_PSRSYNC || GTCCR.TSM) {
    shared_prescaler_counter = 0;
}
```

`PSRSYNC` is a strobe-like bit. Software writes it as one to reset the prescaler; it does not remain meaningfully readable as a persistent one in normal operation.

---

# 5. Common compare, flag, and output model

## 5.1 Counter write behavior

For Timer0/1/3:

```text
write TCNTn:
    counter := written value
    block compare-match detection on the following timer clock
```

This matters for code like:

```c
TCNT0 = OCR0A;
```

That write should not immediately generate `OCF0A`, and the compare match on the next timer clock should be suppressed according to the datasheet’s compare-blocking rule. Timer0 and the 16-bit timers both document this behavior. ([Microchip][1])

Suggested state:

```c
struct TimerCommon {
    bool compare_block_next_tick;
};
```

On a counter write:

```c
timer.counter = value;
timer.compare_block_next_tick = true;
```

On the next timer tick:

```c
bool compare_allowed = !timer.compare_block_next_tick;
timer.compare_block_next_tick = false;
```

## 5.2 Compare-match generation

A compare match occurs when the counting sequence reaches the compare value through timer operation, not merely because software writes a register into an equal state. Timer4 explicitly documents that a software write to the counter that equals an output compare register does not generate a compare match. ([Microchip][1])

For emulation:

```c
if (compare_allowed && counter_reached_ocr_by_timer_counting) {
    set OCFnx;
    apply compare-output action;
}
```

Do not set compare flags just because:

```text
TCNTn == OCRnx after an I/O write
```

## 5.3 Output compare versus physical pin

Each compare channel has an internal output-compare latch. The pin only reflects that latch when:

```text
COMnx bits select output-compare control
and the corresponding DDR bit configures the pin as output
and no higher-priority alternate function overrides it
```

The datasheet states that output compare behavior overrides normal port functionality when enabled, but the data-direction register still controls whether the pin is driven. ([Microchip][1])

For emulation, separate these states:

```c
oc_latch[channel]          // internal compare output state
port_output_latch[pin]     // PORTx bit
ddr[pin]                   // DDRx bit
pin_level[pin]             // final visible pin level
```

Final pin output:

```c
if (ddr_pin_is_output) {
    if (timer_output_override_enabled)
        pin = oc_latch[channel];
    else
        pin = port_output_latch[pin];
} else {
    pin = external_or_pullup_model;
}
```

---

# 6. Timer/Counter0

Timer0 is an 8-bit timer with `TCNT0`, compare registers `OCR0A` and `OCR0B`, compare flags `OCF0A/B`, overflow flag `TOV0`, and output-compare pins `OC0A` and `OC0B`. It supports Normal, CTC, Fast PWM, and Phase Correct PWM modes. ([Microchip][1])

## 6.1 Timer0 pins

| Function | Pin   |
| -------- | ----- |
| `OC0A`   | `PB7` |
| `OC0B`   | `PD0` |
| `T0`     | `PD7` |

The datasheet’s port-function tables show `OC0A` on `PB7`, `OC0B` on `PD0`, and `T0` on `PD7`. ([Microchip][1])

## 6.2 Timer0 registers and bits

### `TCCR0A`

| Bits       | Meaning                           |
| ---------- | --------------------------------- |
| `COM0A1:0` | Compare output mode for channel A |
| `COM0B1:0` | Compare output mode for channel B |
| `WGM01:0`  | Low waveform-generation bits      |

### `TCCR0B`

| Bits     | Meaning                                    |
| -------- | ------------------------------------------ |
| `FOC0A`  | Force output compare A, non-PWM modes only |
| `FOC0B`  | Force output compare B, non-PWM modes only |
| `WGM02`  | High waveform-generation bit               |
| `CS02:0` | Clock select                               |

### `TIMSK0`

| Bit      | Meaning                    |
| -------- | -------------------------- |
| `OCIE0B` | Compare B interrupt enable |
| `OCIE0A` | Compare A interrupt enable |
| `TOIE0`  | Overflow interrupt enable  |

### `TIFR0`

| Bit     | Meaning             |
| ------- | ------------------- |
| `OCF0B` | Compare B flag, W1C |
| `OCF0A` | Compare A flag, W1C |
| `TOV0`  | Overflow flag, W1C  |

Timer0 flags are cleared by writing one to the flag bit. ([Microchip][1])

## 6.3 Timer0 waveform-generation modes

`WGM02:0 = {TCCR0B.WGM02, TCCR0A.WGM01, TCCR0A.WGM00}`.

| Mode | `WGM02:0` | Mode name         | TOP     | OCR update | `TOV0` set |
| ---: | --------- | ----------------- | ------- | ---------- | ---------- |
|    0 | `000`     | Normal            | `0xFF`  | Immediate  | MAX        |
|    1 | `001`     | Phase Correct PWM | `0xFF`  | TOP        | BOTTOM     |
|    2 | `010`     | CTC               | `OCR0A` | Immediate  | MAX        |
|    3 | `011`     | Fast PWM          | `0xFF`  | TOP        | MAX        |
|    4 | `100`     | Reserved          | —       | —          | —          |
|    5 | `101`     | Phase Correct PWM | `OCR0A` | TOP        | BOTTOM     |
|    6 | `110`     | Reserved          | —       | —          | —          |
|    7 | `111`     | Fast PWM          | `OCR0A` | TOP        | TOP        |

The datasheet table defines these TOP sources, OCR update points, and overflow flag points. ([Microchip][1])

## 6.4 Timer0 Normal mode

State:

```text
counter width = 8
BOTTOM = 0x00
MAX = TOP = 0xFF
direction = up
```

On each Timer0 tick:

```c
old = TCNT0;

if (compare_allowed) {
    if (old == OCR0A) set OCF0A and apply COM0A action;
    if (old == OCR0B) set OCF0B and apply COM0B action;
}

if (old == 0xFF) {
    TCNT0 = 0x00;
    set TOV0;
} else {
    TCNT0 = old + 1;
}
```

The overflow flag is set when the counter rolls from `MAX` to `BOTTOM`. ([Microchip][1])

## 6.5 Timer0 CTC mode

State:

```text
TOP = OCR0A
direction = up
OCR0A is not double-buffered in CTC
```

Implementation model:

```c
old = TCNT0;

if (compare_allowed) {
    if (old == OCR0A) {
        set OCF0A;
        apply COM0A action;
    }

    if (old == OCR0B) {
        set OCF0B;
        apply COM0B action;
    }
}

if (old == OCR0A) {
    TCNT0 = 0;
} else if (old == 0xFF) {
    TCNT0 = 0;
    set TOV0;
} else {
    TCNT0 = old + 1;
}
```

Important edge case: because `OCR0A` is not double-buffered in CTC mode, if software writes `OCR0A` to a value lower than the current `TCNT0`, the compare match can be missed until the counter wraps through `MAX`. The datasheet explicitly warns about this missed-match case. ([Microchip][1])

## 6.6 Timer0 Fast PWM

State:

```text
single-slope PWM
BOTTOM = 0x00
TOP = 0xFF in mode 3
TOP = OCR0A in mode 7
OCR0A/OCR0B buffered in PWM modes
```

Counting:

```c
old = TCNT0;

if (old == TOP) {
    TCNT0 = BOTTOM;
    set TOV0;
    load_buffered_OCR_values_that_update_at_TOP();
    apply_bottom_output_actions();
} else {
    TCNT0 = old + 1;
}

if (compare_allowed) {
    if (TCNT0 == OCR0A) set OCF0A and apply COM0A compare action;
    if (TCNT0 == OCR0B) set OCF0B and apply COM0B compare action;
}
```

For non-inverting Fast PWM, the output is cleared on compare match and set at BOTTOM. For inverted Fast PWM, the output is set on compare match and cleared at BOTTOM. The datasheet describes this single-slope behavior and notes that Fast PWM gives a higher PWM frequency than dual-slope PWM. ([Microchip][1])

## 6.7 Timer0 Phase Correct PWM

State:

```text
dual-slope PWM
direction = up or down
TOP = 0xFF in mode 1
TOP = OCR0A in mode 5
OCR updates at TOP
TOV0 set at BOTTOM
```

Implementation:

```c
old = TCNT0;

if (compare_allowed) {
    if (old == OCR0A) {
        set OCF0A;
        if (direction == up) apply_upcount_COM0A_action();
        else                 apply_downcount_COM0A_action();
    }

    if (old == OCR0B) {
        set OCF0B;
        if (direction == up) apply_upcount_COM0B_action();
        else                 apply_downcount_COM0B_action();
    }
}

if (direction == up) {
    if (old == TOP) {
        direction = down;
        load_buffered_OCR_values_that_update_at_TOP();
        TCNT0 = TOP - 1;
    } else {
        TCNT0 = old + 1;
    }
} else {
    if (old == BOTTOM) {
        direction = up;
        set TOV0;
        TCNT0 = BOTTOM + 1;
    } else {
        TCNT0 = old - 1;
    }
}
```

For non-inverting Phase Correct PWM, the output is cleared on compare match while up-counting and set on compare match while down-counting. Inverting mode does the opposite. ([Microchip][1])

## 6.8 Timer0 output compare modes

In non-PWM modes:

| `COM0x1:0` | Behavior                     |
| ---------- | ---------------------------- |
| `00`       | Normal port operation        |
| `01`       | Toggle OC0x on compare match |
| `10`       | Clear OC0x on compare match  |
| `11`       | Set OC0x on compare match    |

In PWM modes, `COM0x1:0 = 10` is non-inverting PWM and `11` is inverting PWM. The toggle mode has special restrictions, especially when `OCR0A` is used as TOP. ([Microchip][1])

## 6.9 Timer0 force compare

Writing one to `FOC0A` or `FOC0B` forces the corresponding output-compare action in non-PWM modes. It does **not** set the compare flag, does **not** clear the counter, and the bit reads back as zero. ([Microchip][1])

Implementation:

```c
if (write_TCCR0B & FOC0A) {
    if (!pwm_mode)
        apply_COM0A_action_as_if_compare_match_but_do_not_set_OCF0A();
}

if (write_TCCR0B & FOC0B) {
    if (!pwm_mode)
        apply_COM0B_action_as_if_compare_match_but_do_not_set_OCF0B();
}
```

---

# 7. Timer/Counter1 and Timer/Counter3

Timer1 and Timer3 are 16-bit timers with three output-compare channels and an input-capture unit. They support Normal, CTC, Fast PWM, Phase Correct PWM, and Phase/Frequency Correct PWM modes. ([Microchip][1])

For implementation, you can share most code between Timer1 and Timer3:

```c
struct Timer16 {
    uint16_t TCNT;
    uint16_t OCR_A, OCR_B, OCR_C;
    uint16_t OCR_A_buf, OCR_B_buf, OCR_C_buf;
    uint16_t ICR;
    uint16_t TEMP;          // shared high-byte temp register
    bool direction_up;
    bool compare_block_next_tick;
    ...
};
```

## 7.1 Timer1/3 pins

| Function       | Timer1 pin    | Timer3 pin                 |
| -------------- | ------------- | -------------------------- |
| `OCnA`         | `PB5`         | `PC6`                      |
| `OCnB`         | `PB6`         | —                          |
| `OCnC`         | `PB7`         | —                          |
| `ICPn`         | `PD4`         | `PC7`                      |
| External clock | `T1` on `PD6` | no external T3 pin on 32U4 |

The port-function tables identify `OC1A/B/C`, `ICP1`, `T1`, `OC3A`, and `ICP3`; the timer clocking section notes that T3 external input is not available on this product. ([Microchip][1])

## 7.2 16-bit register access

The 16-bit timer registers use a shared temporary high-byte latch. Model this per timer.

For 16-bit writes:

```c
write high byte:
    TEMP = value

write low byte:
    target_register = (TEMP << 8) | value
```

For 16-bit reads:

```c
read low byte:
    TEMP = high_byte(target_register)
    return low_byte(target_register)

read high byte:
    return TEMP
```

The temporary high-byte register is shared among the 16-bit registers of the timer. This means an interrupt that accesses another 16-bit register between a low-byte read and high-byte read can corrupt the apparent value. The datasheet warns that 16-bit timer-register accesses must be protected when both main code and ISRs access them. ([Microchip][1])

## 7.3 Timer1/3 clock select

| `CSn2:0` | Clock                     |
| -------- | ------------------------- |
| `000`    | stopped                   |
| `001`    | `clkI/O / 1`              |
| `010`    | `clkI/O / 8`              |
| `011`    | `clkI/O / 64`             |
| `100`    | `clkI/O / 256`            |
| `101`    | `clkI/O / 1024`           |
| `110`    | external Tn, falling edge |
| `111`    | external Tn, rising edge  |

For Timer1, external T1 is available. For Timer3 on the 32U4, external T3 is not available. ([Microchip][1])

## 7.4 Timer1/3 waveform-generation modes

`WGMn3:0 = {TCCRnB.WGMn3, TCCRnB.WGMn2, TCCRnA.WGMn1, TCCRnA.WGMn0}`.

| Mode | `WGMn3:0` | Mode name                   | TOP      | OCR update | `TOVn` set |
| ---: | --------- | --------------------------- | -------- | ---------- | ---------- |
|    0 | `0000`    | Normal                      | `0xFFFF` | Immediate  | MAX        |
|    1 | `0001`    | Phase Correct PWM, 8-bit    | `0x00FF` | TOP        | BOTTOM     |
|    2 | `0010`    | Phase Correct PWM, 9-bit    | `0x01FF` | TOP        | BOTTOM     |
|    3 | `0011`    | Phase Correct PWM, 10-bit   | `0x03FF` | TOP        | BOTTOM     |
|    4 | `0100`    | CTC                         | `OCRnA`  | Immediate  | MAX        |
|    5 | `0101`    | Fast PWM, 8-bit             | `0x00FF` | TOP        | TOP        |
|    6 | `0110`    | Fast PWM, 9-bit             | `0x01FF` | TOP        | TOP        |
|    7 | `0111`    | Fast PWM, 10-bit            | `0x03FF` | TOP        | TOP        |
|    8 | `1000`    | Phase/Frequency Correct PWM | `ICRn`   | BOTTOM     | BOTTOM     |
|    9 | `1001`    | Phase/Frequency Correct PWM | `OCRnA`  | BOTTOM     | BOTTOM     |
|   10 | `1010`    | Phase Correct PWM           | `ICRn`   | TOP        | BOTTOM     |
|   11 | `1011`    | Phase Correct PWM           | `OCRnA`  | TOP        | BOTTOM     |
|   12 | `1100`    | CTC                         | `ICRn`   | Immediate  | MAX        |
|   13 | `1101`    | Reserved                    | —        | —          | —          |
|   14 | `1110`    | Fast PWM                    | `ICRn`   | TOP        | TOP        |
|   15 | `1111`    | Fast PWM                    | `OCRnA`  | TOP        | TOP        |

The datasheet table defines these modes, TOP sources, update points, and overflow flag timing. ([Microchip][1])

## 7.5 TOP source and buffering rules

For Timer1/3, TOP may be fixed, `OCRnA`, or `ICRn`.

Important implementation distinction:

```text
OCRnA as TOP in PWM modes:
    double-buffered

ICRn as TOP:
    not double-buffered
```

The datasheet specifically warns that because `ICRn` is not double-buffered, lowering `ICRn` below the current `TCNTn` can cause the timer to miss TOP and continue counting to `0xFFFF` before wrapping. By contrast, `OCRnA` is double-buffered when used as TOP in PWM modes, making it safer for dynamic frequency changes. ([Microchip][1])

Implementation:

```c
if (mode_is_pwm) {
    write OCRnx:
        OCRnx_buffer = value

    transfer_buffer:
        if update_point_reached:
            OCRnx_active = OCRnx_buffer
} else {
    write OCRnx:
        OCRnx_active = value
}
```

For `ICRn`, do not add a buffer unless the datasheet explicitly defines one for that mode.

## 7.6 Timer1/3 Normal mode

State:

```text
counter width = 16
BOTTOM = 0x0000
MAX = TOP = 0xFFFF
direction = up
```

On each timer tick:

```c
old = TCNTn;

if (compare_allowed) {
    compare old against OCRnA/B/C;
    set OCFnA/B/C as appropriate;
    apply COM actions;
}

if (old == 0xFFFF) {
    TCNTn = 0;
    set TOVn;
} else {
    TCNTn = old + 1;
}
```

## 7.7 Timer1/3 CTC mode

CTC modes:

```text
mode 4:  TOP = OCRnA
mode 12: TOP = ICRn
```

Implementation:

```c
old = TCNTn;
top = active_TOP();

if (compare_allowed) {
    if (old == OCRnA) set OCFnA and apply COMnA;
    if (old == OCRnB) set OCFnB and apply COMnB;
    if (old == OCRnC) set OCFnC and apply COMnC;
}

if (old == top) {
    TCNTn = 0;
} else if (old == 0xFFFF) {
    TCNTn = 0;
    set TOVn;
} else {
    TCNTn = old + 1;
}
```

If TOP is lowered below the current counter in CTC mode, the compare can be missed until the counter reaches `MAX` and wraps. This follows the same unbuffered-TOP hazard described for CTC-style operation. ([Microchip][1])

## 7.8 Timer1/3 Fast PWM

Fast PWM is single-slope.

```text
count BOTTOM -> TOP
then clear to BOTTOM
TOVn set at TOP
OCR update at TOP
```

Implementation:

```c
old = TCNTn;
top = active_TOP();

if (compare_allowed) {
    compare old against OCRnA/B/C;
    set flags and update OC latches;
}

if (old == top) {
    set TOVn;
    transfer_OCR_buffers_that_update_at_TOP();
    TCNTn = BOTTOM;
    apply_bottom_output_actions();
} else {
    TCNTn = old + 1;
}
```

Non-inverting PWM clears the output on compare match and sets it at BOTTOM. Inverting PWM sets the output on compare match and clears it at BOTTOM.

## 7.9 Timer1/3 Phase Correct PWM

Phase Correct PWM is dual-slope.

```text
count BOTTOM -> TOP -> BOTTOM
OCR update at TOP
TOVn set at BOTTOM
```

Implementation:

```c
old = TCNTn;
top = active_TOP();

if (compare_allowed) {
    compare old against OCRnA/B/C;
    if direction_up:
        apply up-count compare-output behavior
    else:
        apply down-count compare-output behavior
}

if (direction_up) {
    if (old == top) {
        direction_up = false;
        transfer_OCR_buffers_that_update_at_TOP();
        TCNTn = top - 1;
    } else {
        TCNTn = old + 1;
    }
} else {
    if (old == BOTTOM) {
        direction_up = true;
        set TOVn;
        TCNTn = BOTTOM + 1;
    } else {
        TCNTn = old - 1;
    }
}
```

## 7.10 Timer1/3 Phase and Frequency Correct PWM

Phase and Frequency Correct PWM is also dual-slope, but the buffer update point differs:

```text
count BOTTOM -> TOP -> BOTTOM
OCR update at BOTTOM
TOVn set at BOTTOM
```

That update-at-BOTTOM behavior is the key semantic difference from ordinary Phase Correct PWM. The datasheet describes the different buffer update timing and overflow behavior for these dual-slope modes. ([Microchip][1])

Implementation:

```c
if (direction_down && old == BOTTOM) {
    set TOVn;
    transfer_OCR_buffers_that_update_at_BOTTOM();
    direction_up = true;
    TCNTn = BOTTOM + 1;
}
```

## 7.11 Timer1/3 compare output modes

In non-PWM modes:

| `COMnx1:0` | Behavior                     |
| ---------- | ---------------------------- |
| `00`       | Normal port operation        |
| `01`       | Toggle OCnx on compare match |
| `10`       | Clear OCnx on compare match  |
| `11`       | Set OCnx on compare match    |

In PWM modes:

| `COMnx1:0` | Behavior              |
| ---------- | --------------------- |
| `00`       | Normal port operation |
| `10`       | Non-inverting PWM     |
| `11`       | Inverting PWM         |

`COMnx = 01` has special toggle behavior in selected PWM modes, particularly when `OCRnA` is used as TOP. Implement this according to the datasheet tables if software relies on timer-generated square waves from TOP compare. ([Microchip][1])

## 7.12 Timer1/3 input capture

Each 16-bit timer has an input-capture unit.

Relevant bits in `TCCRnB`:

| Bit     | Meaning                             |
| ------- | ----------------------------------- |
| `ICNCn` | Input capture noise canceler enable |
| `ICESn` | Input capture edge select           |

Behavior:

```text
selected edge occurs on ICPn:
    ICRn := TCNTn
    set ICFn
    request interrupt if ICIEn and global interrupts enabled
```

If `ICNCn` is set, the input-capture noise canceler requires four equal samples before accepting a transition, adding four system-clock cycles of delay. `ICESn` chooses rising or falling edge. If `ICRn` is being used as TOP, the input-capture function is disconnected. ([Microchip][1])

Implementation model:

```c
sample ICPn every clkI/O cycle

if ICNCn == 0:
    edge_detector_input = sampled_ICPn

if ICNCn == 1:
    shift in sampled_ICPn
    if last_four_samples_equal:
        filtered_ICPn = sampled_value
    edge_detector_input = filtered_ICPn

if selected_edge_detected:
    ICRn = TCNTn
    set ICFn
```

For Timer1, the analog comparator can also be configured as an input-capture trigger path. If you emulate the analog comparator, feed its output into the Timer1 capture unit when that configuration is active. ([Microchip][1])

---

# 8. Timer/Counter4

Timer4 is the most 32U4-specific timer. It is a high-speed 10-bit timer/counter with its own prescaler, optional PLL-derived clocking, compare registers `OCR4A/B/C/D`, TOP defined by `OCR4C`, dead-time generation, complementary output support, enhanced PWM, and fault protection. ([Microchip][1])

## 8.1 Timer4 pins

Timer4 has direct and complementary PWM outputs.

| Channel | Direct output   | Complementary output |
| ------- | --------------- | -------------------- |
| A       | `OC4A` on `PC7` | `/OC4A` on `PC6`     |
| B       | `OC4B` on `PB6` | `/OC4B` on `PB5`     |
| D       | `OC4D` on `PD7` | `/OC4D` on `PD6`     |

These functions share pins with other peripheral functions, so final pin output must go through the normal AVR port override priority model. ([Microchip][1])

## 8.2 Timer4 clock select

Timer4 uses `TCCR4B.CS43:0`.

| `CS43:0` | Timer4 clock divisor |
| -------- | -------------------: |
| `0000`   |              stopped |
| `0001`   |                 `/1` |
| `0010`   |                 `/2` |
| `0011`   |                 `/4` |
| `0100`   |                 `/8` |
| `0101`   |                `/16` |
| `0110`   |                `/32` |
| `0111`   |                `/64` |
| `1000`   |               `/128` |
| `1001`   |               `/256` |
| `1010`   |               `/512` |
| `1011`   |              `/1024` |
| `1100`   |              `/2048` |
| `1101`   |              `/4096` |
| `1110`   |              `/8192` |
| `1111`   |             `/16384` |

Timer4 has its own prescaler, reset by `TCCR4B.PSR4`. It does not use the Timer0/1/3 shared prescaler. ([Microchip][1])

## 8.3 Timer4 source clock and synchronization

Timer4 may run from the synchronous system clock `CK` or from the asynchronous high-speed peripheral clock `PCK`. The datasheet notes that `TCCR4x` and `OCR4x` registers can be read immediately after writing from the CPU side, but internally the values are synchronized into the Timer4 clock domain. Reads of `TCNT4`, `TC4H`, and timer flags are subject to Timer4 synchronization behavior. ([Microchip][1])

A practical emulator should maintain two layers of state:

```c
// CPU-visible register shadow
uint8_t TCCR4A_cpu, TCCR4B_cpu, ...;
uint8_t OCR4A_cpu, OCR4B_cpu, OCR4C_cpu, OCR4D_cpu;

// Timer4-domain active state
uint16_t OCR4A_active, OCR4B_active, OCR4C_active, OCR4D_active;
uint16_t TCNT4_active;
```

For many programs, it is sufficient to transfer CPU writes into Timer4 active state on the next Timer4 source tick. For very strict emulation, especially if software writes a Timer4 register and immediately polls `TCNT4` or a flag, implement explicit synchronization queues according to the Timer4 synchronization diagrams.

## 8.4 Timer4 10-bit register access through `TC4H`

Timer4’s counter and compare registers are physically exposed as 8-bit low registers plus a shared high-bit register, `TC4H`.

Write sequence:

```c
write TC4H = high_bits;
write OCR4A/OCR4B/OCR4C/OCR4D/TCNT4 = low_byte;

committed_value = (TC4H_bits << 8) | low_byte;
```

Read sequence:

```c
read low byte first:
    returns low byte
    latches high bits into TC4H

read TC4H:
    returns previously latched high bits
```

`TC4H` is shared across Timer4’s 10-bit registers, so an ISR touching Timer4 between the high-byte and low-byte portions of an access can corrupt the access. The datasheet describes this explicit `TC4H` access protocol for 10-bit Timer4 registers. 

Implementation detail:

```c
uint8_t TC4H_cpu;

write_TC4H(v):
    TC4H_cpu = v & 0x03;      // normal 10-bit mode

write_OCR4x_low(v):
    OCR4x_cpu = ((TC4H_cpu & 0x03) << 8) | v;
```

For enhanced mode, `TC4H` participates in the enhanced compare/PWM mechanism, so preserve the relevant high/enhanced bits rather than blindly masking too aggressively if you emulate `ENHC4`.

## 8.5 `OCR4C` as TOP

Unlike Timer0/1/3, Timer4 uses `OCR4C` as the period register in its main modes.

```text
TOP = OCR4C
```

A compare match with `OCR4C` clears `TCNT4`. The datasheet notes that `OCR4C` values below three are automatically replaced by three, so your emulator should clamp active TOP to at least `3`. ([Microchip][1])

Implementation:

```c
write_OCR4C(value):
    OCR4C_cpu = value
    OCR4C_active = max(value, 3)
```

If using synchronization queues, clamp when the value reaches the Timer4 domain.

## 8.6 Timer4 mode selection

Timer4 mode selection depends on the channel PWM enable bits and `TCCR4D.WGM41:0`.

| PWM enabled? | `WGM41:0` | Mode                              |
| ------------ | --------- | --------------------------------- |
| `PWM4x = 0`  | any       | Normal operation for that channel |
| `PWM4x = 1`  | `00`      | Fast PWM                          |
| `PWM4x = 1`  | `01`      | Phase and Frequency Correct PWM   |
| `PWM4x = 1`  | `10`      | PWM6 single-slope                 |
| `PWM4x = 1`  | `11`      | PWM6 dual-slope                   |

The datasheet defines Timer4’s modes through the `PWM4x`, `WGM41:0`, and `COM4x` configuration fields. ([Microchip][1])

For emulation, Timer4’s counter mode is effectively global, while output behavior is channel-specific. A useful model is:

```c
bool any_pwm_enabled = PWM4A || PWM4B || PWM4D;
mode = decode(WGM41_0, any_pwm_enabled);
```

Then per channel:

```c
if (!PWM4x)
    channel behaves as normal compare output
else
    channel uses PWM behavior selected by WGM41:0
```

## 8.7 Timer4 Normal mode

State:

```text
counter width = 10
BOTTOM = 0
TOP = OCR4C
direction = up
```

On each Timer4 prescaled tick:

```c
old = TCNT4;

if (old == OCR4A) set OCF4A and apply COM4A action;
if (old == OCR4B) set OCF4B and apply COM4B action;
if (old == OCR4D) set OCF4D and apply COM4D action;

if (old == OCR4C) {
    TCNT4 = 0;
    set TOV4;
} else {
    TCNT4 = old + 1;
}
```

Timer4 compare matches are generated when the counter reaches the compare values through counting, and `OCR4C` defines the clear-on-match period behavior. ([Microchip][1])

## 8.8 Timer4 Fast PWM

Fast PWM is single-slope:

```text
count BOTTOM -> TOP
TOP = OCR4C
then clear to BOTTOM
TOV4 set at TOP
OCR update at TOP
```

Implementation:

```c
old = TCNT4;

if (compare_allowed) {
    compare old against OCR4A/B/D;
    set OCF4A/B/D;
    update OC4A/B/D latches;
}

if (old == OCR4C) {
    set TOV4;
    transfer_buffered_OCR4_values_at_TOP();
    TCNT4 = 0;
    apply_bottom_output_actions();
} else {
    TCNT4 = old + 1;
}
```

For non-inverting PWM, the output compare waveform is cleared on compare match and set at BOTTOM. Timer4’s Fast PWM behavior and output compare waveform behavior are described in the datasheet’s Timer4 mode section. ([Microchip][1])

## 8.9 Timer4 Phase and Frequency Correct PWM

Timer4 Phase and Frequency Correct PWM is dual-slope:

```text
count BOTTOM -> TOP -> BOTTOM
TOP = OCR4C
TOV4 set at BOTTOM
OCR update at BOTTOM
```

Implementation:

```c
old = TCNT4;

if (compare_allowed) {
    compare old against OCR4A/B/D;
    if direction_up:
        apply up-count PWM compare behavior
    else:
        apply down-count PWM compare behavior
}

if (direction_up) {
    if (old == OCR4C) {
        direction_up = false;
        TCNT4 = OCR4C - 1;
    } else {
        TCNT4 = old + 1;
    }
} else {
    if (old == 0) {
        set TOV4;
        transfer_buffered_OCR4_values_at_BOTTOM();
        direction_up = true;
        TCNT4 = 1;
    } else {
        TCNT4 = old - 1;
    }
}
```

The datasheet describes Timer4’s dual-slope counting sequence and its compare-output behavior for Phase and Frequency Correct PWM. ([Microchip][1])

## 8.10 Timer4 PWM6

PWM6 mode is intended to generate six PWM outputs: direct and complementary outputs for channels A, B, and D. The `OC4OE` bits in `TCCR4E` gate whether each PWM6 output is actually connected to the pin. If an `OC4OE` bit is clear, normal port behavior applies for that pin. ([Microchip][1])

PWM6 modes:

```text
WGM41:0 = 10 -> PWM6 single-slope
WGM41:0 = 11 -> PWM6 dual-slope
```

For PWM6 pin gating:

| Bit      | Output gate         |
| -------- | ------------------- |
| `OC4OE0` | one A-side output   |
| `OC4OE1` | other A-side output |
| `OC4OE2` | one B-side output   |
| `OC4OE3` | other B-side output |
| `OC4OE4` | one D-side output   |
| `OC4OE5` | other D-side output |

Because the datasheet’s PWM6 table names pins by physical port location, implement the mapping from the official table rather than assuming bit order from channel names alone. ([Microchip][1])

## 8.11 Timer4 compare-output modes

Timer4 has more output configurations than the ordinary timers because it supports complementary output pairs.

A clean emulator should represent, per channel:

```c
struct Timer4Channel {
    bool direct_latch;
    bool complement_latch;
    uint8_t COM;
    bool PWM_enabled;
    bool output_enable_direct;
    bool output_enable_complement;
};
```

Then derive physical pins from:

```c
if normal mode:
    use non-complementary OC4x behavior

if PWM/complementary mode:
    update direct and complementary latches according to COM4x mode
    apply dead-time if enabled/configured
    gate through OC4OE in PWM6
    finally pass through DDR/PORT override model
```

The datasheet provides separate compare-output tables for Timer4 normal operation, PWM operation, complementary PWM operation, and PWM6 operation. ([Microchip][1])

## 8.12 Timer4 dead-time

Timer4 has a dead-time register `DT4`.

| Bits      | Meaning             |
| --------- | ------------------- |
| `DT4H3:0` | High-side dead time |
| `DT4L3:0` | Low-side dead time  |

`TCCR4B.DTPS41:40` selects a dead-time prescaler. Dead time can range over 4-bit values, prescaled by the dead-time prescaler. The feature is used to prevent complementary outputs from switching both sides of a half-bridge on simultaneously. ([Microchip][1])

If your emulator exposes OC pins to software but does not emulate analog power electronics, you still need to delay the visible complementary pin transitions if software polls those pins or if another emulated device observes them.

Suggested model:

```c
on requested complementary transition:
    do not immediately switch both pins

    turn off currently-active side immediately or according to table
    schedule opposite side to turn on after dead_time_ticks
```

Dead-time ticks should be measured in Timer4 dead-time prescaler ticks, derived from the Timer4 clock domain.

## 8.13 Timer4 enhanced PWM

`TCCR4E.ENHC4` enables Enhanced Compare/PWM mode. In this mode, compare channels `OCR4A`, `OCR4B`, and `OCR4D` gain enhanced resolution/accuracy behavior, while `OCR4C` remains the period/TOP register. ([Microchip][1])

For emulator staging:

```text
minimum implementation:
    store ENHC4
    do ordinary Timer4 compare behavior

accurate implementation:
    model enhanced compare bit from TC4H/OCR4x access
    adjust compare edge timing/output update according to enhanced mode
```

Most Arduino/Arduboy-style software is unlikely to rely on enhanced Timer4 sub-bit PWM accuracy, but a primary implementation reference should preserve the state and not discard `ENHC4`.

## 8.14 Timer4 synchronous update lock

`TCCR4E.TLOCK4` locks Timer4 compare-register updates so multiple PWM values can be written and then released together. When `TLOCK4` is set, writes are held; when it is cleared, the locked updates become active together according to the Timer4 update rules. ([Microchip][1])

Implementation:

```c
if (TLOCK4) {
    write OCR4x:
        OCR4x_locked_shadow = value
        OCR4x_locked_dirty = true
} else {
    write OCR4x:
        normal Timer4 write/buffer behavior
}

on clearing TLOCK4:
    for each dirty OCR4x:
        transfer locked shadow into normal Timer4 buffer path
```

## 8.15 Timer4 fault protection

Timer4 has a fault-protection unit controlled by `TCCR4D`.

Relevant bits:

| Bit       | Meaning                                   |
| --------- | ----------------------------------------- |
| `FPIE4`   | Fault protection interrupt enable         |
| `FPEN4`   | Fault protection enable                   |
| `FPNC4`   | Fault protection noise canceler           |
| `FPES4`   | Fault protection edge select              |
| `FPAC4`   | Fault protection analog comparator select |
| `FPF4`    | Fault protection flag                     |
| `WGM41:0` | Timer4 waveform mode bits                 |

A fault can be triggered through the selected fault source, optionally noise-canceled. On a fault, Timer4 disables PWM output control, clears fault enable behavior as specified, sets the fault flag, and can request the Timer4 fault-protection interrupt. 

Implementation:

```c
if (FPEN4 && selected_fault_edge_detected) {
    FPF4 = 1;

    disable_timer4_output_compare_drive_as_specified();
    FPEN4 = 0;

    if (FPIE4 && SREG_I)
        request TIMER4_FPF interrupt;
}
```

`FPF4` is cleared by writing one to it.

---

# 9. Power reduction and sleep behavior

When a timer’s `PRTIMx` bit is set, freeze the timer state and block normal register access to that peripheral. When the bit is cleared, resume from the frozen state. ([Microchip][1])

In Idle sleep, the CPU clock is stopped but peripheral clocks such as timers may continue, allowing timers to wake the CPU by interrupt. In deeper sleep modes, timer behavior depends on which clocks remain active; because the 32U4 lacks asynchronous Timer2, it does not have the classic Timer2 power-save asynchronous wake behavior. ([Microchip][1])

For emulator purposes:

```c
if CPU sleeping in Idle:
    do not execute CPU instructions
    continue advancing timers
    if enabled timer interrupt becomes pending:
        wake CPU and vector if global interrupt rules allow

if power-down:
    stop clkI/O-driven timers
    Timer4 behavior depends on whether its clock source remains active in your clock/power model
```

---

# 10. Recommended implementation architecture

## 10.1 Timer state structures

```c
typedef struct {
    uint8_t tccr_a, tccr_b;
    uint8_t tcnt;
    uint8_t ocr_a, ocr_b;
    uint8_t ocr_a_buf, ocr_b_buf;
    uint8_t timsk;
    uint8_t tifr;

    bool direction_up;
    bool compare_block_next_tick;

    bool oc_a_latch;
    bool oc_b_latch;
} Timer0;
```

```c
typedef struct {
    uint8_t tccr_a, tccr_b, tccr_c;
    uint16_t tcnt;
    uint16_t ocr_a, ocr_b, ocr_c;
    uint16_t ocr_a_buf, ocr_b_buf, ocr_c_buf;
    uint16_t icr;
    uint16_t temp;

    uint8_t timsk;
    uint8_t tifr;

    bool direction_up;
    bool compare_block_next_tick;

    bool oc_a_latch;
    bool oc_b_latch;
    bool oc_c_latch;

    bool icp_last;
    uint8_t icp_filter_shift;
} Timer16;
```

```c
typedef struct {
    uint8_t tccr_a, tccr_b, tccr_c, tccr_d, tccr_e;
    uint16_t tcnt;
    uint16_t ocr_a, ocr_b, ocr_c, ocr_d;
    uint16_t ocr_a_buf, ocr_b_buf, ocr_c_buf, ocr_d_buf;

    uint8_t tc4h;
    uint8_t timsk;
    uint8_t tifr;
    uint8_t dt4;

    bool direction_up;

    bool oc_a_direct, oc_a_comp;
    bool oc_b_direct, oc_b_comp;
    bool oc_d_direct, oc_d_comp;

    uint32_t prescaler_phase;
    double source_phase;

    // Optional synchronization queues:
    SyncQueue cpu_to_timer4;
    SyncQueue timer4_to_cpu;
} Timer4;
```

## 10.2 I/O write dispatcher

At minimum, special-case these writes:

```text
TIFR0/1/3/4:
    clear flags where written bit = 1

TCNT0:
    write counter
    block next compare

TCNT1/3 high/low:
    use 16-bit TEMP protocol
    block next compare on committed low-byte write

OCR registers:
    direct or buffered depending on mode

ICR registers:
    16-bit TEMP protocol
    no PWM TOP buffering unless explicitly defined

TCCR registers:
    store writable bits
    force-compare bits are strobes
    reserved bits read as zero

GTCCR:
    handle TSM and prescaler reset strobes

TCCR4B.PSR4:
    reset Timer4 prescaler

TC4H + Timer4 low register:
    implement 10-bit access protocol

TCCR4E.TLOCK4:
    lock/release Timer4 compare updates
```

## 10.3 Timer tick dispatcher

```c
void tick_timers_one_cpu_cycle(void) {
    sample_external_timer_inputs();

    if (!GTCCR_TSM)
        tick_shared_prescaler_and_timer0_1_3();

    tick_timer4_clock_domain();

    update_interrupt_requests();
}
```

For Timer0/1/3:

```c
void tick_shared_prescaler_and_timer0_1_3(void) {
    shared_prescaler++;

    if (timer0_clock_edge())
        timer0_tick();

    if (timer1_clock_edge())
        timer16_tick(&timer1);

    if (timer3_clock_edge())
        timer16_tick(&timer3);
}
```

For Timer4:

```c
void tick_timer4_clock_domain(void) {
    timer4.source_phase += timer4_source_hz / cpu_hz;

    while (timer4.source_phase >= 1.0) {
        timer4.source_phase -= 1.0;

        if (timer4_prescaler_edge())
            timer4_tick();
    }
}
```

---

# 11. Edge cases worth testing

Use these as emulator regression tests.

| Test                                              | Expected behavior                                             |
| ------------------------------------------------- | ------------------------------------------------------------- |
| Timer0 Normal, no prescale, `TCNT0 = 0`           | `TOV0` after 256 timer ticks                                  |
| Timer0 CTC, `OCR0A = 10`                          | Counter sequence `0..10,0..10`                                |
| Timer0 CTC, lower `OCR0A` below current `TCNT0`   | Match may be missed until wrap                                |
| Write `TCNT0 = OCR0A`                             | No immediate `OCF0A`; next compare blocked                    |
| Timer1 16-bit read low then high                  | High byte comes from TEMP latched by low-byte read            |
| Timer1 high write then low write                  | Register commits only on low-byte write                       |
| Timer1 input capture with noise canceler          | Edge delayed until four equal samples                         |
| Timer3 external clock mode                        | No real T3 external input on 32U4                             |
| Timer4 `OCR4C < 3`                                | Active TOP becomes 3                                          |
| Timer4 at 64 MHz PCK, CPU 16 MHz                  | Four Timer4 source ticks per CPU cycle before Timer4 prescale |
| Timer4 `TC4H` write followed by `OCR4A` low write | 10-bit OCR4A receives high bits from `TC4H`                   |
| Timer flag W1C                                    | Writing one clears; writing zero leaves unchanged             |
| `SBI TIFR0, OCF0A` hazard                         | May clear other flags set during read-modify-write            |

---

# 12. Accuracy caveats

A few behaviors are inherently awkward to model at pure CPU-cycle granularity:

1. **External T0/T1 clocking** has a documented synchronization delay of about 2.5 to 3.5 system-clock cycles. A deterministic sampled-edge model is usually best: sample each `clkI/O`, run a small synchronizer pipeline, then emit a timer tick. ([Microchip][1])

2. **Timer4 asynchronous PCK synchronization** can expose subtle timing if software writes Timer4 registers and immediately reads `TCNT4` or flags. For best accuracy, maintain CPU-visible shadows and Timer4-domain active registers with synchronization delay queues. ([Microchip][1])

3. **Timer4 dead-time/complementary PWM** only matters if your emulator exposes physical pin waveforms to software or to another emulated device. If you only need timer interrupts, you can initially preserve the registers and skip analog-style pin timing, but a full pin-level emulator should schedule dead-time-delayed output transitions.

4. **Reserved modes and reserved bits** should not invent behavior. Store only writable bits, read reserved bits as zero, and avoid triggering undocumented side effects.

The most important implementation split is: **Timer0/1/3 are conventional synchronous AVR timers sharing a prescaler**, while **Timer4 is a separate high-speed timer with its own clock-domain and PWM machinery**.

[1]: https://ww1.microchip.com/downloads/en/devicedoc/atmel-7766-8-bit-avr-atmega16u4-32u4_datasheet.pdf "ATmega16U4/32U4 Datasheet"
