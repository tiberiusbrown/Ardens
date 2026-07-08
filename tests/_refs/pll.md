## AVR ATmega32U4 PLL overview

The ATmega32U4 PLL is a clock-generation block, not a CPU clock source. Its job is to generate a high-frequency internal clock for two consumers: the USB controller and the high-speed Timer/Counter4. The datasheet describes the PLL as generating up to 96 MHz from a nominal 8 MHz PLL reference, then sending that output through independent postscalers for USB and Timer4. ([Microchip][1])

For emulator purposes, model the PLL as:

```text
clock source / internal RC
        |
PLL input mux + optional /2 prescaler
        |
8 MHz reference into PLL
        |
PLL VCO/output frequency selected by PDIV[3:0]
        |
        +--> USB postscaler: /1 or /2
        |
        +--> Timer4 postscaler: disconnected, /1, /1.5, /2
```

The PLL has no interrupt of its own. Software observes it primarily through `PLLCSR.PLOCK`, and consumers observe it indirectly through the availability and frequency of `clkUSB` and Timer4’s asynchronous PLL clock.

---

# 1. Clock domains affected

## PLL input reference

The PLL requires an 8 MHz input reference. The reference can come from either:

1. the main clock-source path through the PLL input prescaler, selected with `PINMUX = 0`, or
2. the internal calibrated 8 MHz RC oscillator directly, selected with `PINMUX = 1`.

The datasheet states that the PLL prescaler supports an 8 MHz source directly or a 16 MHz source divided by 2, and that the PLL input multiplexer can instead select the internal calibrated 8 MHz RC oscillator. ([Microchip][1])

Important emulator point: the PLL is not fed by the CPU clock after `CLKPR`. The clock distribution shows the PLL clock prescaler and PLL input mux as separate from the system clock prescaler. So changing `CLKPR` should slow the CPU and synchronous peripherals, but it should not change the PLL’s reference frequency. ([Microchip][1])

## PLL output

The PLL output frequency is selected by `PLLFRQ.PDIV3:0`. Valid output frequencies are:

| `PDIV3:0` | PLL output |
| --------: | ---------: |
|    `0011` |     40 MHz |
|    `0100` |     48 MHz |
|    `0101` |     56 MHz |
|    `0111` |     72 MHz |
|    `1000` |     80 MHz |
|    `1001` |     88 MHz |
|    `1010` |     96 MHz |

All other `PDIV3:0` values are listed as “not allowed.” The reset value of `PLLFRQ` is `0b0000_0100`, so the default programmed PLL output frequency is 48 MHz, although the PLL is disabled after reset. ([Microchip][1])

## USB clock

The USB controller needs a 48 MHz reference for full-speed operation. The 32U4 USB chapter states that full-speed USB requires a 48 MHz ±0.25% reference clock from the internal PLL. Crystal-less operation using the internal RC oscillator is only specified for USB low-speed mode; for full-speed USB, an external crystal oscillator or external source clock is required. ([Microchip][1])

For emulation:

```text
if PLLE == 1 and PLOCK == 1:
    clkUSB = pll_output / (PLLUSB ? 2 : 1)
else:
    clkUSB = stopped/unavailable
```

Typical valid USB configurations:

| PLL output | `PLLUSB` | `clkUSB` |
| ---------: | -------: | -------: |
|     48 MHz |        0 |   48 MHz |
|     96 MHz |        1 |   48 MHz |

Other values are writable and can produce a non-48 MHz USB clock. For a strict USB timing model, such configurations should make USB malfunction or fail timing. For a firmware-oriented emulator, it is usually enough to expose the derived frequency and let the USB peripheral model decide whether it can operate.

## Timer4 PLL clock

Timer/Counter4 can use either a synchronous system-derived clock or an asynchronous PLL-derived clock. The Timer4 section says the timer clock is generated using the Timer4 clock-select bits plus the `PLLTM1:0` postscaler bits in `PLLFRQ`. It also gives a PLL initialization sequence for asynchronous mode: enable the PLL, wait 100 µs, poll `PLOCK`, then configure `PLLTM1:0` to a value other than `00`. ([Microchip][1])

For emulation:

```text
if PLLE == 1 and PLOCK == 1:
    if PLLTM == 00: pll_timer_clock = disconnected
    if PLLTM == 01: pll_timer_clock = pll_output / 1
    if PLLTM == 10: pll_timer_clock = pll_output / 1.5
    if PLLTM == 11: pll_timer_clock = pll_output / 2
else:
    pll_timer_clock = disconnected/stopped
```

The datasheet notes that the `/1.5` Timer4 postscaler introduces jitter while maintaining zero average frequency error and 50% average duty cycle. ([Microchip][1])

For a cycle-level Timer4 emulator, implement `/1.5` as a fractional divider that emits two Timer4 clock ticks for every three PLL input/output cycles. A simple event model is:

```c
// For PLLTM = /1.5
acc += 2;
if (acc >= 3) {
    acc -= 3;
    emit_timer4_pll_tick();
}
```

This gives alternating 1-cycle and 2-cycle spacing, average divide-by-1.5. Reset the fractional divider phase when the PLL locks, when `PLLTM` changes, or when Timer4 switches into the PLL clock domain.

---

# 2. Register map

The relevant PLL registers are ordinary AVR I/O registers:

| Register | I/O address | data-space address |  reset |
| -------- | ----------: | -----------------: | -----: |
| `PLLCSR` |      `0x29` |             `0x49` | `0x00` |
| `PLLFRQ` |      `0x32` |             `0x52` | `0x04` |

The register summary lists `PLLCSR` at `0x29 (0x49)` and `PLLFRQ` at `0x32 (0x52)`. ([Microchip][1])

Because these are I/O addresses above `0x1F`, they are accessible by `IN`/`OUT` and by data-space `LD`/`ST` forms, but not by `SBI`/`CBI`, which only operate on I/O addresses `0x00–0x1F`.

---

# 3. `PLLCSR` — PLL Control and Status Register

```text
I/O addr:   0x29
Data addr:  0x49
Reset:      0x00

Bit:        7   6   5   4       3   2   1     0
Name:       -   -   -   PINDIV  -   -   PLLE  PLOCK
Access:     R   R   R   R/W     R   R   R/W   R
Reset:      0   0   0   0       0   0   0     0
```

The datasheet defines bits 7:5 and 3:2 as reserved and always read as zero. `PINDIV` selects the PLL input prescaler for 8 MHz versus 16 MHz source input, `PLLE` starts the PLL, and `PLOCK` reports that the PLL is locked. ([Microchip][1])

## Bit 4 — `PINDIV`: PLL input prescaler

`PINDIV` is meaningful when `PLLFRQ.PINMUX = 0`, meaning the PLL input comes from the primary clock-source path through the PLL prescaler.

| `PINDIV` | intended source |  PLL input |
| -------: | --------------: | ---------: |
|        0 |    8 MHz source | source / 1 |
|        1 |   16 MHz source | source / 2 |

The datasheet says this bit must be `0` for an 8 MHz clock source and `1` for a 16 MHz clock source before enabling the PLL. ([Microchip][1])

When `PINMUX = 1`, the PLL input is the internal calibrated 8 MHz RC oscillator directly, so `PINDIV` should be ignored for frequency calculation.

## Bit 1 — `PLLE`: PLL enable

When software writes `PLLE = 1`, the PLL starts. If `PINMUX = 1`, the calibrated internal 8 MHz RC oscillator is automatically enabled while `PLLE` is set. The datasheet specifically warns that the PLL must be disabled before entering power-down mode to stop that internal RC oscillator and avoid extra consumption. ([Microchip][1])

For emulation:

```c
on_write_PLLCSR(value):
    old_PLLE = PLLE;
    PINDIV = bit(value, 4);
    PLLE   = bit(value, 1);

    if (!PLLE) {
        PLOCK = 0;
        pll_running = false;
        cancel_pending_lock();
    } else if (!old_PLLE) {
        start_pll_lock_sequence();
    }
```

Changing `PINDIV` while `PLLE = 1` is not a recommended datasheet sequence. A practical emulator should treat this as a reference-clock change: clear `PLOCK`, restart the lock timer, and recompute the output if the configuration is valid.

## Bit 0 — `PLOCK`: PLL lock detector

`PLOCK` is read-only. It becomes `1` when the PLL is locked to the reference clock. The datasheet says that after enabling the PLL it takes “several ms” to lock, and that clearing `PLLE` clears `PLOCK`. ([Microchip][1])

For emulation:

```c
read_PLLCSR():
    return (PINDIV << 4) | (PLLE << 1) | (PLOCK ? 1 : 0);
```

A write to bit 0 should not directly set or clear `PLOCK`. Only hardware state changes should affect it.

---

# 4. `PLLFRQ` — PLL Frequency Control Register

```text
I/O addr:   0x32
Data addr:  0x52
Reset:      0x04

Bit:        7       6       5       4       3      2      1      0
Name:       PINMUX  PLLUSB  PLLTM1  PLLTM0  PDIV3  PDIV2  PDIV1  PDIV0
Access:     R/W     R/W     R/W     R/W     R/W    R/W    R/W    R/W
Reset:      0       0       0       0       0      1      0      0
```

The datasheet defines all bits in `PLLFRQ` as read/write, with initial value `0x04`. ([Microchip][1])

## Bit 7 — `PINMUX`: PLL input multiplexer

| `PINMUX` | PLL reference source                                 |
| -------: | ---------------------------------------------------- |
|        0 | PLL prescaler fed by the primary system clock source |
|        1 | internal calibrated 8 MHz RC oscillator directly     |

The datasheet says `PINMUX = 1` supports USB low-speed crystal-less operation or crystals other than 8/16 MHz; full-speed USB still requires an external crystal oscillator or external source clock. ([Microchip][1])

Emulator behavior:

```c
if (PINMUX == 0) {
    pll_ref = primary_clock_source / (PINDIV ? 2 : 1);
} else {
    pll_ref = internal_rc_8mhz;
}
```

If the emulated reference is not approximately 8 MHz, a strict analog-faithful model should prevent lock. A firmware-compatibility model can either still lock or expose a configuration warning.

## Bit 6 — `PLLUSB`: USB postscaler

| `PLLUSB` | USB clock                 |
| -------: | ------------------------- |
|        0 | `clkUSB = pll_output`     |
|        1 | `clkUSB = pll_output / 2` |

The datasheet explicitly frames these as the two useful cases: no division when PLL output is 48 MHz, or divide-by-2 when PLL output is 96 MHz. ([Microchip][1])

## Bits 5:4 — `PLLTM1:0`: Timer4 postscaler

| `PLLTM1:0` | Timer4 PLL postscaler |
| ---------: | --------------------- |
|       `00` | disconnected          |
|       `01` | divide by 1           |
|       `10` | divide by 1.5         |
|       `11` | divide by 2           |

`00` means Timer4’s PLL clock is not supplied. This does not necessarily stop Timer4 if Timer4 is configured to use another clock source.

## Bits 3:0 — `PDIV3:0`: PLL output frequency

| `PDIV3:0` | PLL output frequency |
| --------: | -------------------: |
|    `0000` |          not allowed |
|    `0001` |          not allowed |
|    `0010` |          not allowed |
|    `0011` |               40 MHz |
|    `0100` |               48 MHz |
|    `0101` |               56 MHz |
|    `0110` |          not allowed |
|    `0111` |               72 MHz |
|    `1000` |               80 MHz |
|    `1001` |               88 MHz |
|    `1010` |               96 MHz |
|    `1011` |          not allowed |
|    `1100` |          not allowed |
|    `1101` |          not allowed |
|    `1110` |          not allowed |
|    `1111` |          not allowed |

The datasheet calls the 96 MHz configuration optimal at 5 V, with `/1.5` for a 64 MHz Timer4 clock and `/2` for a 48 MHz USB clock. ([Microchip][1])

---

# 5. PLL state machine for an emulator

A useful digital state model is:

```text
DISABLED
    PLLE = 0
    PLOCK = 0
    no PLL output

STARTING
    PLLE = 1
    PLOCK = 0
    lock timer running
    consumers see no valid PLL clock, or see unstable clock if modeling analog behavior

LOCKED
    PLLE = 1
    PLOCK = 1
    pll_output available
    clkUSB and Timer4 PLL clocks available according to postscalers

INVALID_CONFIG
    PLLE = 1
    PLOCK = 0
    illegal PDIV or invalid 8 MHz reference
    consumers see no valid PLL clock
```

Suggested transitions:

```c
on_reset:
    PLLCSR = 0x00;       // visible writable bits clear
    PLLFRQ = 0x04;       // PDIV = 0100 => 48 MHz programmed
    PLLE = 0;
    PLOCK = 0;
    state = DISABLED;

on_PLLE_rising_edge:
    PLOCK = 0;
    if (pll_config_valid())
        state = STARTING, schedule_lock_event();
    else
        state = INVALID_CONFIG;

on_lock_event:
    if (PLLE && pll_config_valid())
        PLOCK = 1, state = LOCKED;

on_PLLE_cleared:
    PLOCK = 0;
    state = DISABLED;
    cancel_lock_event();

on_PDIV_or_PINMUX_or_PINDIV_change_while_enabled:
    PLOCK = 0;
    if (pll_config_valid())
        state = STARTING, schedule_lock_event();
    else
        state = INVALID_CONFIG;

on_PLLUSB_or_PLLTM_change:
    // no PLL relock required; just recompute consumer clocks
    recompute_usb_and_timer4_clocks();
```

The exact analog lock time is not specified cycle-by-cycle. The datasheet says “several ms,” while the Timer4 initialization sequence says to wait 100 µs and then poll `PLOCK`. For an emulator, make the lock delay configurable. For firmware compatibility, the crucial behavior is that `PLOCK` is initially clear after enabling and eventually becomes set while `PLLE` remains set and the configuration is valid. ([Microchip][1])

---

# 6. Recommended frequency computation

```c
static int pll_output_hz_from_pdiv(uint8_t pdiv)
{
    switch (pdiv & 0x0f) {
    case 0x3: return 40000000;
    case 0x4: return 48000000;
    case 0x5: return 56000000;
    case 0x7: return 72000000;
    case 0x8: return 80000000;
    case 0x9: return 88000000;
    case 0xA: return 96000000;
    default:  return 0;        // not allowed
    }
}

static bool pll_config_valid(void)
{
    if (!PLLE)
        return false;

    if (pll_output_hz_from_pdiv(PDIV) == 0)
        return false;

    if (PINMUX == 0) {
        int ref = primary_clock_source_hz / (PINDIV ? 2 : 1);
        return ref == 8000000; // or tolerance-based
    } else {
        return true;           // internal RC modeled as 8 MHz
    }
}

static int pll_output_hz(void)
{
    if (!PLLE || !PLOCK)
        return 0;
    return pll_output_hz_from_pdiv(PDIV);
}

static int usb_clock_hz(void)
{
    int f = pll_output_hz();
    if (!f)
        return 0;
    return PLLUSB ? f / 2 : f;
}

static Rational timer4_pll_clock_hz(void)
{
    int f = pll_output_hz();

    if (!f)
        return Rational{0, 1};

    switch (PLLTM) {
    case 0: return Rational{0, 1};      // disconnected
    case 1: return Rational{f, 1};      // /1
    case 2: return Rational{2*f, 3};    // /1.5
    case 3: return Rational{f, 2};      // /2
    }
}
```

---

# 7. Software-visible behavior summary

A good emulator should implement at least these behaviors:

| Behavior                     | Required emulator behavior                                                            |
| ---------------------------- | ------------------------------------------------------------------------------------- |
| Reset                        | `PLLCSR = 0x00`, `PLLFRQ = 0x04`, `PLOCK = 0`, PLL off                                |
| Write reserved `PLLCSR` bits | Ignore; read back zero                                                                |
| Write `PLLCSR.PLOCK`         | Ignore; read-only                                                                     |
| Set `PLLCSR.PLLE`            | Start lock sequence; `PLOCK` remains 0 until lock delay completes                     |
| Clear `PLLCSR.PLLE`          | Stop PLL and immediately clear `PLOCK`                                                |
| `PINMUX = 0`                 | Reference comes from main clock-source path via `PINDIV`                              |
| `PINMUX = 1`                 | Reference comes from internal calibrated 8 MHz RC; `PINDIV` does not affect frequency |
| Illegal `PDIV`               | Keep register value, but do not lock or mark PLL output invalid                       |
| `PLLUSB` change              | Immediately changes derived USB clock if PLL is locked                                |
| `PLLTM` change               | Immediately changes Timer4 PLL clock if PLL is locked                                 |
| `/1.5` Timer4 mode           | Generate 2 ticks per 3 PLL cycles, with jitter/alternating spacing                    |
| `CLKPR` changes              | Do not change PLL reference frequency                                                 |
| PLL interrupts               | None; software polls `PLOCK`                                                          |

For typical Arduino Leonardo / ATmega32U4 full-speed USB operation with a 16 MHz crystal, firmware commonly configures:

```c
PLLCSR = (1 << PINDIV) | (1 << PLLE);  // 16 MHz source /2 => 8 MHz PLL input
PLLFRQ = (1 << PLLUSB) | 0x0A;         // 96 MHz PLL, USB = 96/2 = 48 MHz
while (!(PLLCSR & (1 << PLOCK))) {
    // wait
}
```

For Timer4 at 64 MHz from the same 96 MHz PLL:

```c
PLLFRQ = (1 << PLLUSB) | (2 << PLLTM0) | 0x0A;
// PLLTM1:0 = 10 => /1.5
// Timer4 PLL clock = 96 MHz / 1.5 = 64 MHz
```

The precise bit names in C headers may vary by toolchain style, but the hardware fields above are the datasheet fields.

[1]: https://ww1.microchip.com/downloads/en/devicedoc/atmel-7766-8-bit-avr-atmega16u4-32u4_datasheet.pdf "ATmega16U4/32U4 Datasheet"
