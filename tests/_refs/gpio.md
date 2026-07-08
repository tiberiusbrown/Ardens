Below is a cycle-accurate-emulator-oriented overview of the classic AVR GPIO port system, with ATmega16U4/ATmega32U4 details called out. Most of this applies to other “classic” megaAVR devices, but exact port masks, alternate functions, and interrupt pins are device-specific.

## 1. High-level model

An AVR I/O pin is not just a CPU-visible bit. For emulation, model each pin as a small pipeline:

```text
CPU I/O register access
    ↓
DDRx / PORTx / PINx side effects
    ↓
alternate-function override muxes
    ↓
output driver / pull-up / input buffer
    ↓
physical pad and external circuit resolution
    ↓
digital input synchronizer
    ↓
PINx read value, pin-change/external interrupt logic, peripheral inputs
```

On the ATmega32U4 there are **26 programmable I/O lines**, distributed across ports B, C, D, E, and F. The datasheet describes each AVR port as having three I/O memory locations: `DDRx`, `PORTx`, and `PINx`; the ports support true read-modify-write behavior, individually selectable pull-ups, and alternate-function multiplexing. ([Microchip][1])

The most important distinction for emulation is:

```text
read PORTx  → reads the PORT output/pull-up latch
read DDRx   → reads the direction latch
read PINx   → reads the synchronized physical pin state
write PINx  → toggles the corresponding PORT latch bits where written bits are 1
```

`PINx` is therefore not a normal read/write storage register. ([Microchip][1])

---

## 2. The three GPIO registers

For each port `x`, each bit `n` has three main CPU-visible bits:

```text
DDxn    in DDRx
PORTxn  in PORTx
PINxn   in PINx
```

### `DDRx`: data direction register

`DDxn = 0` makes the pin an input.

`DDxn = 1` makes the pin an output.

On reset, port pins are tri-stated: `DDRx = 0`, `PORTx = 0`. ([Microchip][1])

### `PORTx`: output latch or pull-up-enable latch

The meaning of `PORTxn` depends on `DDxn`.

| `DDxn` | `PORTxn` | Meaning                                                            |
| -----: | -------: | ------------------------------------------------------------------ |
|      0 |        0 | Input, internal pull-up disabled, high impedance                   |
|      0 |        1 | Input, internal pull-up enabled, unless globally disabled by `PUD` |
|      1 |        0 | Output, drive low                                                  |
|      1 |        1 | Output, drive high                                                 |

The `PUD` bit in `MCUCR` globally disables internal pull-ups. If `PUD = 1`, then even `{DDxn, PORTxn} = {0, 1}` behaves as high-impedance input rather than input-with-pull-up. ([Microchip][1])

### `PINx`: synchronized input read, write-one-to-toggle

Reading `PINx` returns the input synchronizer value for the physical pad, not the `PORTx` latch.

Writing `1` to a bit in `PINx` toggles the corresponding `PORTx` bit. Writing `0` has no effect. This works regardless of whether the pin is currently configured as input or output. ([Microchip][1])

So:

```c
PORTB = 0b00000001;
PINB  = 0b00000001;   // toggles PORTB0
// PORTB is now 0b00000000
```

And:

```asm
sbi PINB, 0    ; toggles PORTB0
cbi PINB, 0    ; does not toggle; writes zero, so no effect
```

This is a common source of emulator bugs.

---

## 3. ATmega32U4 GPIO register map

The 32U4 GPIO registers are in low I/O space, so they are accessible through `IN`, `OUT`, `SBI`, `CBI`, `SBIS`, and `SBIC`. For `LD`/`ST` data-space access, add `0x20` to the I/O address. The datasheet explicitly notes this I/O-vs-data-space offset and the availability of bit instructions for I/O addresses `0x00`–`0x1F`. ([Microchip][1])

| Port | Physical bits on ATmega32U4 | `PINx` I/O / data | `DDRx` I/O / data | `PORTx` I/O / data |
| ---- | --------------------------: | ----------------: | ----------------: | -----------------: |
| B    |                         7:0 |   `0x03` / `0x23` |   `0x04` / `0x24` |    `0x05` / `0x25` |
| C    |                         7:6 |   `0x06` / `0x26` |   `0x07` / `0x27` |    `0x08` / `0x28` |
| D    |                         7:0 |   `0x09` / `0x29` |   `0x0A` / `0x2A` |    `0x0B` / `0x2B` |
| E    |                        6, 2 |   `0x0C` / `0x2C` |   `0x0D` / `0x2D` |    `0x0E` / `0x2E` |
| F    |                    7:4, 1:0 |   `0x0F` / `0x2F` |   `0x10` / `0x30` |    `0x11` / `0x31` |

`MCUCR` contains `PUD` at bit 4. On the 32U4, `MCUCR` is at I/O address `0x35`, data-space address `0x55`; it is reachable by `IN`/`OUT`, but not by `SBI`/`CBI` because those only address I/O `0x00`–`0x1F`. ([Microchip][1])

For absent port bits, the datasheet marks bits as reserved or not implemented. A strict device model should mask absent `PINx` bits to zero and should not create physical pads for them. For `DDRx`/`PORTx`, you can either store all eight bits internally for software readback stability or mask reserved bits to zero; the stricter datasheet-faithful behavior is to treat reserved bits as zero/unused, but most real code should not depend on them.

Recommended masks:

```c
PORTB_MASK = 0b11111111;
PORTC_MASK = 0b11000000;
PORTD_MASK = 0b11111111;
PORTE_MASK = 0b01000100;   // PE6, PE2
PORTF_MASK = 0b11110011;   // PF7:4, PF1:0
```

---

## 4. CPU instruction timing for port access

For instruction-level or cycle-level emulation, the GPIO registers must be integrated with AVR I/O instruction timing.

Important instruction cycle counts:

| Instruction | Use                           |     Cycles |
| ----------- | ----------------------------- | ---------: |
| `IN Rd, A`  | read I/O register             |          1 |
| `OUT A, Rr` | write I/O register            |          1 |
| `SBI A, b`  | set bit in low I/O register   |          2 |
| `CBI A, b`  | clear bit in low I/O register |          2 |
| `SBIS A, b` | skip if bit set               | 1, 2, or 3 |
| `SBIC A, b` | skip if bit clear             | 1, 2, or 3 |

`IN` and `OUT` are single-cycle. `SBI` and `CBI` are two-cycle read-modify-write-style I/O bit operations. `SBIS`/`SBIC` take extra cycles depending on whether a skip occurs and whether the skipped instruction is one or two words. ([Microchip][1])

A practical model:

```text
OUT PORTx, r
    cycle completes
    PORTx latch is updated
    output driver state changes for subsequent pad evaluation

IN r, PORTx
    samples the PORT latch directly in that instruction

IN r, PINx
    samples the synchronized pin input, not the immediate pad
```

The subtle part is that changing `PORTx` or `DDRx` can change the physical pad immediately for the next pad-resolution step, but a `PINx` read sees the value only after the input synchronizer delay.

---

## 5. `PINx` synchronization and read latency

The AVR does not feed the raw asynchronous pad directly to the CPU. The pin value goes through synchronization logic. The datasheet describes an external pin transition as having a delay of **0.5 to 1.5 system clock cycles** before it is visible through `PINx`. It also gives the classic software-output readback example:

```asm
out PORTx, r16
nop
in  r17, PINx
```

The `nop` is needed when software writes a port and then wants to read the resulting physical pin state through `PINx`. ([Microchip][1])

For emulation, distinguish these cases:

### Reading `PORTx`

No synchronizer. Reads the latch.

```asm
out PORTB, r16
in  r17, PORTB     ; should see the new latch immediately
```

### Reading `PINx` after writing `PORTx`

Synchronizer delay applies.

```asm
out PORTB, r16
in  r17, PINB      ; may still see previous synchronized pad state
nop
in  r18, PINB      ; should now see the new pad state if no external conflict
```

### Reading an output pin externally forced low

If `DDRB0 = 1` and `PORTB0 = 1`, the AVR output driver attempts to drive high. But if your external circuit model forces the pad low, then:

```text
read PORTB0 → 1
read PINB0  → 0 after synchronization
```

This distinction matters for software that probes stuck pins, shared buses, or emulated peripherals.

A reasonable cycle model is:

```c
// Internal latches
uint8_t port_latch;
uint8_t ddr_latch;

// Physical pad after resolving AVR drive, pullups, and external devices
uint8_t pad_level;

// Synchronized CPU-visible PINx value
uint8_t pin_sync;

// On each I/O clock edge:
pin_sync_next = resolved_digital_pad_level;
```

To match the datasheet’s 0.5–1.5 cycle behavior more closely, update the synchronizer on CPU/I/O clock edges and make asynchronous external transitions visible on the next appropriate edge. For a simpler instruction-level emulator, treat `PINx` as one CPU cycle delayed from the pad.

---

## 6. Port state transitions and pull-ups

The basic configuration table is:

| `DDxn` | `PORTxn` | `PUD` | Pin behavior           |
| -----: | -------: | ----: | ---------------------- |
|      0 |        0 |     X | Input, high-Z          |
|      0 |        1 |     0 | Input, pull-up enabled |
|      0 |        1 |     1 | Input, high-Z          |
|      1 |        0 |     X | Output low             |
|      1 |        1 |     X | Output high            |

The internal pull-up is weak. The datasheet gives the I/O pull-up resistance as roughly **20–50 kΩ**, depending on voltage, temperature, and process. ([Microchip][1])

For a digital emulator, represent pin drive strength with at least:

```text
strong 0
strong 1
weak 1   // internal pull-up
Z        // high impedance
external drive(s)
```

Resolution can be:

```text
strong 0 + strong 1 → contention / X / chosen policy
strong 0 + weak 1   → 0
strong 1 + weak 1   → 1
weak 1 only         → 1
Z only              → floating / previous / external default
```

For most software-visible behavior, it is enough to resolve to digital `0`, `1`, or optionally `X`. Current draw and damage behavior are normally not CPU-visible unless you are modeling analog effects, ADC readings, or faults.

---

## 7. Safe switching sequences

The datasheet warns that switching between input/high-Z and output-high or between input-pull-up and output-low can pass through intermediate states. For example, moving from high-Z input `{DD, PORT} = {0,0}` to output-high `{1,1}` necessarily passes through either input-pull-up `{0,1}` or output-low `{1,0}`, depending on whether software writes `PORTx` or `DDRx` first. ([Microchip][1])

For an emulator, this means you should not collapse consecutive writes into a final pin state. Each I/O write can create an externally visible transient state.

Example:

```asm
; Start: DDxn=0, PORTxn=0  input high-Z

sbi PORTB, 0   ; now input with pull-up
sbi DDRB, 0    ; now output high
```

versus:

```asm
; Start: DDxn=0, PORTxn=0  input high-Z

sbi DDRB, 0    ; now output low
sbi PORTB, 0   ; now output high
```

The second sequence briefly drives low.

---

## 8. True read-modify-write behavior

AVR ports are designed so that bit operations affect one pin without unintentionally modifying other pins. `SBI` and `CBI` on `PORTx` or `DDRx` should preserve all other bits. ([Microchip][1])

For example:

```asm
sbi PORTB, 3
```

should be equivalent to:

```c
PORTB = PORTB | (1 << 3);
```

except that the instruction takes exactly two cycles and is atomic with respect to interrupt servicing; AVR interrupts are not taken in the middle of an instruction.

But remember the special `PINx` behavior:

```asm
sbi PINB, 3    ; toggles PORTB3
cbi PINB, 3    ; no effect
```

A full-byte write to `PINx` toggles every bit written as `1`:

```c
PINB = 0xFF;   // toggles all PORTB latch bits
PINB = 0x01;   // toggles only PORTB0
```

This also means that C code like this is dangerous on real AVR hardware:

```c
PINB |= (1 << 0);
```

Because it first reads `PINB`, which returns physical pin levels, then writes back those levels ORed with the mask, thereby toggling every currently-high pin bit.

---

## 9. Alternate-function override model

Most 32U4 pins are multiplexed with alternate functions: timers, SPI, USART, TWI, ADC, analog comparator, JTAG, external interrupts, and USB-related pins. The datasheet models these with generic override signals such as:

```text
PUOE / PUOV   pull-up override enable / value
DDOE / DDOV   data-direction override enable / value
PVOE / PVOV   port-value override enable / value
PTOE          port toggle override
DIEOE / DIEOV digital-input enable override enable / value
DI            digital input after Schmitt trigger
AIO           analog input/output directly connected to pad
```

When no alternate function overrides a pin, the normal `DDRx`, `PORTx`, and `PUD` behavior applies. When a peripheral override is active, it can force output-enable, output value, pull-up state, digital-input enable, or analog connection independently of the CPU-visible latch bits. The CPU-visible `DDRx` and `PORTx` registers still retain their stored values. ([Microchip][1])

A useful emulator structure is:

```c
// Register latches
bool dd    = bit(DDRx, n);
bool port  = bit(PORTx, n);
bool pud   = bit(MCUCR, PUD);

// Peripheral override signals for this pin
bool puoe, puov;
bool ddoe, ddov;
bool pvoe, pvov;
bool ptoe;
bool dieoe, dieov;

// Effective pull-up
bool pullup_enabled =
    puoe ? puov :
           (!dd && port && !pud);

// Effective output enable
bool output_enabled =
    ddoe ? ddov : dd;

// Effective output value
bool output_value =
    pvoe ? pvov : port;

// Optional timer/peripheral toggle override
if (ptoe)
    output_value = !output_value;

// Effective digital input buffer enable
bool digital_input_enabled =
    dieoe ? dieov : default_digital_input_enabled_for_sleep_and_pin;
```

Do not implement alternate functions by directly modifying `PORTx` or `DDRx`. They override the pad behavior, not necessarily the register contents.

---

## 10. Important ATmega32U4 alternate pin functions

Abbreviated map:

### Port B

| Pin | Common alternate functions        |
| --- | --------------------------------- |
| PB7 | `OC0A`, `OC1C`, `PCINT7`, `RTS`   |
| PB6 | `OC1B`, `PCINT6`, `OC4B`, `ADC13` |
| PB5 | `OC1A`, `PCINT5`, `OC4B`, `ADC12` |
| PB4 | `PCINT4`, `ADC11`                 |
| PB3 | `PDO/MISO`, `PCINT3`              |
| PB2 | `PDI/MOSI`, `PCINT2`              |
| PB1 | `SCK`, `PCINT1`                   |
| PB0 | `SS`, `PCINT0`                    |

([Microchip][1])

### Port C

| Pin | Common alternate functions |
| --- | -------------------------- |
| PC7 | `ICP3`, `CLKO`, `OC4A`     |
| PC6 | `OC3A`, `OC4A`             |

([Microchip][1])

### Port D

| Pin | Common alternate functions |
| --- | -------------------------- |
| PD7 | `T0`, `OC4D`, `ADC10`      |
| PD6 | `T1`, `OC4D`, `ADC9`       |
| PD5 | `XCK1`, `CTS`              |
| PD4 | `ICP1`, `ADC8`             |
| PD3 | `INT3`, `TXD1`             |
| PD2 | `INT2`, `RXD1`             |
| PD1 | `INT1`, `SDA`              |
| PD0 | `INT0`, `SCL`, `OC0B`      |

([Microchip][1])

### Port E and F

| Pin | Common alternate functions |
| --- | -------------------------- |
| PE6 | `INT6`, `AIN0`             |
| PE2 | `HWB`                      |
| PF7 | `ADC7`, `TDI`              |
| PF6 | `ADC6`, `TDO`              |
| PF5 | `ADC5`, `TMS`              |
| PF4 | `ADC4`, `TCK`              |
| PF1 | `ADC1`                     |
| PF0 | `ADC0`                     |

JTAG is particularly important: when JTAG is enabled, the JTAG pins on Port F are not ordinary GPIO, and some pull-ups can be active even during reset. ([Microchip][1])

---

## 11. TWI/I²C pins are special

On the 32U4, TWI uses:

```text
PD1 = SDA
PD0 = SCL
```

When TWI is enabled, these pins are disconnected from ordinary port control in the sense that the TWI peripheral takes over the pad behavior. The datasheet describes the TWI pins as open-drain / slew-rate-limited and including spike filtering that suppresses spikes shorter than 50 ns. ([Microchip][1])

For emulation:

```text
TWI disabled:
    PD0/PD1 behave as ordinary GPIO

TWI enabled:
    pin output behavior is open-drain:
        drive low or release
    high level comes from pull-up / external bus
    PORTD/DDRD latches still exist but do not directly drive the pins
```

This matters for software that switches between bit-banged I²C and hardware TWI, or that reads the physical pins while the TWI peripheral is active.

---

## 12. Timer output compare pins

Timer output compare units can override the pin output value, but on classic AVR devices the corresponding `DDR` bit commonly still has to be set for the waveform to appear on the physical pin.

For example, if `OC1A` is enabled on PB5 but `DDB5 = 0`, the timer may internally toggle its compare output state, but the pad may remain an input unless the port override table for that function says otherwise. So do not assume “timer enabled” automatically means “pin drives output” unless the datasheet’s override logic for that pin says so.

For external timer clock inputs such as `T0` and `T1`, the external input is sampled once per system clock. The datasheet says the edge detector introduces a delay of roughly **2.5 to 3.5 system clock cycles** before the counter is updated, and recommends the external clock be below `f_clk_IO / 2.5`. ([Microchip][1])

That timing is separate from ordinary `PINx` CPU reads.

---

## 13. External interrupts and pin-change interrupts

The 32U4 has external interrupts on:

```text
INT0 = PD0
INT1 = PD1
INT2 = PD2
INT3 = PD3
INT6 = PE6
```

It also has pin-change interrupts on:

```text
PCINT7:0 = PB7:PB0
```

The datasheet states that external interrupts and pin-change interrupts can trigger even if the pin is configured as an output. This allows software-generated interrupts by changing the output value. ([Microchip][1])

### External interrupt sense modes

For `INT3:0`, sense control supports:

| Sense bits | Mode               |
| ---------: | ------------------ |
|       `00` | Low level          |
|       `01` | Any logical change |
|       `10` | Falling edge       |
|       `11` | Rising edge        |

For `INT6`, sense modes are similar, but the datasheet notes specific sampling behavior for edge detection; pulses shorter than one clock are not guaranteed to generate an interrupt. ([Microchip][1])

### Pin-change interrupt behavior

`PCICR.PCIE0` enables the pin-change interrupt group. `PCMSK0` masks individual `PCINT7:0` bits. If any enabled pin changes logical state, `PCIFR.PCIF0` is set. The flag is cleared by executing the interrupt vector or by writing a one to the flag bit. ([Microchip][1])

For emulation, model PCINT approximately as:

```c
uint8_t observed = digital_pad_state_for_port_b & PCMSK0;
uint8_t changed  = observed ^ previous_observed;

if (PCICR_PCIE0 && changed)
    PCIFR_PCIF0 = 1;

previous_observed = observed;
```

The hard question is whether to use the raw digital pad, the Schmitt-triggered `DI` signal, or the synchronized `PINx` value. For more accurate behavior, interrupts should observe the pin’s digital input path, not a software read of `PINx`. Asynchronous wake-capable interrupts should not be delayed exactly the same way as `PINx` CPU reads.

---

## 14. Digital input disable registers

Analog-capable pins can have their digital input buffers disabled through `DIDR` registers. When disabled, the corresponding `PINx` bit reads as zero, while the analog function can still use the pad. On the 32U4 this affects ADC pins and the analog comparator input `AIN0` on PE6. ([Microchip][1])

For emulation:

```c
if (digital_input_disabled_for_pin)
    PINx_read_bit = 0;
else
    PINx_read_bit = synchronized_pad_bit;
```

This is visible to software and should be implemented.

---

## 15. Sleep-mode pin behavior

In deeper sleep modes, the digital input signal can be clamped to ground to avoid power consumption from floating or mid-level inputs. The datasheet notes exceptions for enabled external interrupts and certain alternate functions that must continue operating during sleep. ([Microchip][1])

For a cycle-accurate emulator, sleep affects pins in two separate ways:

```text
1. CPU and I/O clocks may stop or slow.
2. Digital input buffers may be disabled/clamped except for wake-capable pins.
```

So a sleeping AVR may not update `PINx` synchronizers normally, but asynchronous interrupt logic may still detect wake conditions on selected pins.

---

## 16. Recommended emulator state

For each port:

```c
struct AvrPort {
    uint8_t ddr;        // DDRx latch
    uint8_t port;       // PORTx latch
    uint8_t pin_sync;   // CPU-visible synchronized PINx value
    uint8_t pin_prev;   // previous digital state, useful for interrupts
    uint8_t mask;       // physically implemented pins
};
```

For each pin:

```c
struct AvrPin {
    bool pad_level;          // resolved physical digital level
    Drive avr_drive;         // strong0, strong1, weak1, Z
    Drive external_drive;    // from board/peripheral model
    bool digital_enabled;
    bool analog_connected;
};
```

A useful per-cycle order is:

```text
1. Execute CPU cycle or instruction micro-step.
2. Commit I/O writes at the correct point in the instruction.
3. Update DDRx/PORTx/PINx side effects.
4. Let peripherals compute alternate-function overrides.
5. Resolve physical pad levels from AVR drive + external circuit.
6. Update asynchronous interrupt detectors.
7. On I/O clock edge, update PINx synchronizer state.
8. At instruction boundary, evaluate interrupt entry rules.
```

For many emulators, instruction-level timing is sufficient if these visible rules are preserved:

```text
PORTx read after PORTx write sees the new latch immediately.
PINx read after PORTx write needs a one-cycle/NOP delay.
PINx write toggles PORTx bits.
External pin changes are not necessarily visible until the next synchronizer update.
Alternate functions override pads, not the stored PORTx/DDRx latches.
```

---

## 17. Register access pseudocode

```c
uint8_t read_port_register(Port *p, Reg reg) {
    switch (reg) {
    case REG_DDR:
        return p->ddr & p->implemented_mask;

    case REG_PORT:
        return p->port & p->implemented_mask;

    case REG_PIN:
        return p->pin_sync & p->implemented_mask;

    default:
        unreachable();
    }
}

void write_port_register(Port *p, Reg reg, uint8_t value) {
    value &= p->implemented_mask;

    switch (reg) {
    case REG_DDR:
        p->ddr = value;
        break;

    case REG_PORT:
        p->port = value;
        break;

    case REG_PIN:
        // write-one-to-toggle PORT latch
        p->port ^= value;
        break;
    }
}
```

For looser compatibility with software that writes reserved bits and reads them back, store all 8 bits in `ddr` and `port`, but apply `implemented_mask` only when computing physical pads and when reading `PINx`. For stricter 32U4 behavior, mask on both writes and reads.

---

## 18. Pin drive computation

```c
Drive compute_avr_drive(Avr *avr, Port *p, int bit) {
    bool dd   = (p->ddr  >> bit) & 1;
    bool port = (p->port >> bit) & 1;
    bool pud  = avr->MCUCR & MCUCR_PUD;

    Override o = compute_alternate_override(avr, p, bit);

    bool pullup =
        o.puoe ? o.puov :
                 (!dd && port && !pud);

    bool oe =
        o.ddoe ? o.ddov : dd;

    bool val =
        o.pvoe ? o.pvov : port;

    if (o.ptoe)
        val = !val;

    if (oe)
        return val ? DRIVE_STRONG_1 : DRIVE_STRONG_0;

    if (pullup)
        return DRIVE_WEAK_1;

    return DRIVE_Z;
}
```

Then resolve with external devices:

```c
bool resolve_pad(Drive avr, Drive external) {
    // Example simple digital policy
    if (avr == DRIVE_STRONG_0 || external == DRIVE_STRONG_0)
        return 0;

    if (avr == DRIVE_STRONG_1 || external == DRIVE_STRONG_1)
        return 1;

    if (avr == DRIVE_WEAK_1 || external == DRIVE_WEAK_1)
        return 1;

    return floating_default_or_previous_value;
}
```

For open-drain buses such as I²C/TWI, use wired-AND resolution:

```text
any device drives low → bus low
no device drives low + pull-up → bus high
no drive and no pull-up → floating
```

---

## 19. Good emulator conformance tests

These catch most GPIO mistakes.

### `PORTx` latch readback

```asm
ldi r16, 0x55
out PORTB, r16
in  r17, PORTB
; r17 should be 0x55
```

### `PINx` delayed readback

```asm
sbi DDRB, 0
sbi PORTB, 0
in  r17, PINB      ; may still be old
nop
in  r18, PINB      ; should show PB0 high if not externally forced
```

### `PINx` write-one-to-toggle

```asm
ldi r16, 0x01
out PINB, r16      ; toggles PORTB0
```

### Input pull-up

```asm
cbi DDRB, 0
sbi PORTB, 0       ; input pull-up
; if no external low, PINB0 should eventually read high
```

### Global pull-up disable

```asm
sbi PORTB, 0       ; input pull-up requested
; set MCUCR.PUD
; PB0 should no longer be weakly pulled high internally
```

### Output externally forced low

```text
DDB0 = 1
PORTB0 = 1
external device drives PB0 low

read PORTB0 → 1
read PINB0  → 0 after synchronization
```

### Pin-change interrupt from output toggle

```text
PB0 configured output
PCICR.PCIE0 = 1
PCMSK0.PCINT0 = 1
toggle PORTB0
if physical/logical PB0 changes, PCIFR.PCIF0 should set
```

### Digital input disable

```text
Set DIDR0 bit for ADC0/PF0.
Read PINF0.
Expected: zero, regardless of pad level.
```

---

## 20. Summary rules

For a cycle-accurate AVR GPIO emulator, the essential rules are:

1. `DDRx` controls direction latch.
2. `PORTx` controls output value or input pull-up latch.
3. `PINx` reads synchronized physical pin state.
4. Writing `1` to `PINx` toggles the corresponding `PORTx` latch.
5. `PORTx` reads the latch, not the pin.
6. `PINx` reads can lag physical pin changes by about one clock.
7. `OUT`/`IN` are one cycle; `SBI`/`CBI` are two cycles.
8. Each I/O write can create an externally visible transient pin state.
9. Pull-ups are weak and globally disabled by `MCUCR.PUD`.
10. Alternate functions override pad behavior, not necessarily register storage.
11. External and pin-change interrupts can fire even when a pin is configured as output.
12. Digital-input-disable bits can force `PINx` reads to zero on analog-capable pins.
13. Sleep modes can clamp or disable digital input paths except for wake-capable pins.
14. For the 32U4, only ports B, C, D, E, and F exist, and several ports are sparse.

[1]: https://ww1.microchip.com/downloads/en/devicedoc/atmel-7766-8-bit-avr-atmega16u4-32u4_datasheet.pdf "ATmega16U4/32U4 Datasheet"
