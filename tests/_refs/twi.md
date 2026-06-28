Below is an **emulator-oriented expansion** of the ATmega32U4 TWI/I²C peripheral. I would model the TWI peripheral as a **bus-connected finite-state machine with cycle-scheduled SCL/SDA pin activity**, not merely as a byte-level I²C helper.

The main caveat: the datasheet gives the externally visible state machine, register behavior, status codes, and SCL frequency formula, but it does **not** fully specify every internal half-cycle phase, synchronizer latency, or exact cycle on which `TWINT` becomes visible. So a good emulator should implement a deterministic timing model that matches the datasheet and then, for true cycle accuracy, calibrate the last few cycle offsets against hardware.

## 1. What “cycle accurate” should mean here

For TWI, you probably need three layers:

1. **CPU-visible register accuracy**
   Reads/writes of `TWBR`, `TWSR`, `TWAR`, `TWDR`, `TWCR`, and `TWAMR` must behave correctly, including write-one-to-clear `TWINT`, `TWWC`, `TWSTO` auto-clear, and status-code sequencing.

2. **Pin-level bus accuracy**
   `PD0/SCL` and `PD1/SDA` must behave as open-drain TWI pins when `TWEN=1`. External devices, other emulated Arduboys, and GPIO reads must see plausible SCL/SDA transitions at the correct cycle boundaries. The datasheet says the TWI pins include slew-rate limiting and spike suppression, and that spikes shorter than 50 ns are filtered when TWI is enabled. ([Microchip][1])

3. **Clock/state-machine timing accuracy**
   A master byte transfer should take the correct number of CPU cycles according to `TWBR` and `TWPS`, plus stretching and arbitration delays. The datasheet gives the master SCL formula and says slave mode does not depend on the bit-rate generator, though the slave CPU clock must be at least 16 times the SCL frequency. ([Microchip][1])

For most AVR firmware, the important observable events are:

```text
write TWCR clearing TWINT
→ TWI state machine runs
→ pins change over time
→ status code changes
→ TWINT becomes 1
→ optional interrupt is requested
→ SCL may be held low until software responds
```

That last point is essential: when `TWINT` is set, the TWI transfer is suspended, and the SCL low period is stretched until software clears `TWINT`. The datasheet explicitly says clearing `TWINT` starts the next TWI operation, so `TWDR`, `TWAR`, and `TWSR` accesses must be complete before clearing it. ([Microchip][1])

---

# 2. Core data structures to model

A useful internal TWI model might look like this:

```c
typedef enum {
    TWI_DISABLED,

    TWI_IDLE_UNADDRESSED_SLAVE,
    TWI_WAIT_BUS_FREE_FOR_START,

    TWI_MASTER_START,
    TWI_MASTER_REPEATED_START,
    TWI_MASTER_SEND_BYTE,
    TWI_MASTER_RECV_BYTE,
    TWI_MASTER_STOP,

    TWI_SLAVE_ADDR_RECV,
    TWI_SLAVE_RECV_BYTE,
    TWI_SLAVE_SEND_BYTE,

    TWI_BUS_ERROR,
    TWI_HALTED_TWINT
} TWI_State;

typedef enum {
    TWI_ROLE_NONE,
    TWI_MASTER_TRANSMITTER,
    TWI_MASTER_RECEIVER,
    TWI_SLAVE_RECEIVER,
    TWI_SLAVE_TRANSMITTER
} TWI_Role;

typedef struct {
    uint8_t TWBR;    // 0xB8
    uint8_t TWSR;    // 0xB9, status in bits 7:3, prescaler in bits 1:0
    uint8_t TWAR;    // 0xBA
    uint8_t TWDR;    // 0xBB
    uint8_t TWCR;    // 0xBC
    uint8_t TWAMR;   // 0xBD

    TWI_State state;
    TWI_Role  role;

    uint8_t status;       // masked status, e.g. 0x08, 0x18, 0xF8
    uint8_t shift_reg;
    uint8_t bit_index;    // 0..7 for data/address bits, 8 for ACK/NACK
    bool ack_bit;

    bool addressed;
    bool general_call_match;
    bool bus_busy;
    bool arbitration_lost;

    bool drive_scl_low;
    bool drive_sda_low;

    uint32_t phase_countdown;
    bool scl_phase_high;
    bool waiting_for_scl_release;
} TWI;
```

The actual register addresses are `TWBR=0xB8`, `TWSR=0xB9`, `TWAR=0xBA`, `TWDR=0xBB`, `TWCR=0xBC`, and `TWAMR=0xBD`. ([Microchip][1])

---

# 3. Pin and bus model

## 3.1 Open-drain behavior

Model each participant on the bus as contributing two booleans:

```c
device_drive_scl_low
device_drive_sda_low
```

Then resolve the actual bus levels:

```c
SCL = !any_device_drive_scl_low;
SDA = !any_device_drive_sda_low;
```

This is required for:

* ACK/NACK behavior.
* Slave clock stretching.
* Multi-master arbitration.
* Firmware that polls physical pin levels.
* TWI plus GPIO interaction.

The bus is wired-AND: multiple masters’ clocks synchronize so that the combined SCL high period is determined by the shortest master high period and the low period by the longest low period. Masters listen to SCL and start their high/low timing from the actual bus transitions. ([Microchip][1])

## 3.2 TWI ownership of PD0/PD1

On ATmega32U4:

```text
PD0 = SCL
PD1 = SDA
```

When `TWEN=1`, the TWI peripheral takes control of the SCL/SDA pins, enabling the TWI pin behavior. When `TWEN=0`, the TWI is switched off and ongoing TWI transfers are terminated. ([Microchip][1])

So in your GPIO model:

```c
if (TWEN && !PRTWI) {
    pin_PD0_output_low = twi.drive_scl_low;
    pin_PD1_output_low = twi.drive_sda_low;

    // Optional: PORTD pull-up bits can still enable weak pullups.
} else {
    // Ordinary GPIO DDRD/PORTD behavior owns the pins.
}
```

The datasheet also says `PRR0.PRTWI` must be zero to enable the 2-wire serial interface. ([Microchip][1])

## 3.3 Spike filtering

The datasheet says spikes shorter than 50 ns are suppressed on SCL/SDA when TWI is enabled. ([Microchip][1])

For a 16 MHz AVR, one CPU cycle is 62.5 ns, so a whole-cycle pulse is already longer than 50 ns. If your emulator has only CPU-cycle resolution, a one-cycle pulse should generally be visible. If your bus model has sub-cycle timing, ignore pulses whose duration is less than 50 ns before feeding them to the TWI edge detector.

---

# 4. Master timing model

The datasheet gives:

```text
SCL frequency = f_CPU / (16 + 2 * TWBR * 4^TWPS)
```

where `TWPS` is selected by `TWSR.TWPS1:0`, with prescaler values 1, 4, 16, and 64. ([Microchip][1])

For emulation, define:

```c
prescaler = {1, 4, 16, 64}[TWSR & 0x03];
scl_period_cycles = 16 + 2 * TWBR * prescaler;
half_period_cycles = 8 + TWBR * prescaler;
```

The **total period** is datasheet-defined. The equal split into low/high halves is an emulator modeling choice. It is the most natural interpretation of the formula and works well for software-visible behavior, but the datasheet does not explicitly guarantee the exact internal phase split.

The datasheet warns that `TWBR` should be 10 or higher in master mode; if lower, the master may produce incorrect SDA/SCL output for the remainder of the byte during a START + SLA + R/W sequence. ([Microchip][1])

For a cycle-level emulator, I would implement this as:

```c
if (master_mode && TWBR < 10) {
    // Optional hardware-quirk mode:
    // Either warn, or emulate undefined/incorrect output only if you have
    // hardware measurements. Most emulators should not invent a random fault.
}
```

---

# 5. Register semantics

## `TWCR`

Layout:

```text
bit 7  TWINT
bit 6  TWEA
bit 5  TWSTA
bit 4  TWSTO
bit 3  TWWC
bit 2  TWEN
bit 1  reserved, reads 0
bit 0  TWIE
```

Important side effects:

### `TWINT`

`TWINT` is set by hardware when the TWI finishes its current operation and needs software response. If `TWIE` and global interrupts are enabled, this requests the TWI interrupt. While `TWINT` is set, the TWI stretches SCL low. Software clears `TWINT` by writing a logic 1 to it; it is not automatically cleared on interrupt entry. ([Microchip][1])

Emulator behavior:

```c
write_TWCR(value) {
    bool clear_twint = value & (1 << TWINT);

    // Store writable control bits, except special cases.
    TWEA  = value & (1 << TWEA);
    TWSTA = value & (1 << TWSTA);
    TWSTO = value & (1 << TWSTO);
    TWEN  = value & (1 << TWEN);
    TWIE  = value & (1 << TWIE);

    if (clear_twint) {
        TWINT = 0;
        begin_next_twi_operation_from_current_status_and_control_bits();
    }
}
```

### `TWEA`

`TWEA` controls ACK generation. If set, the peripheral ACKs:

* its own slave address,
* general call if enabled in `TWAR.TWGCE`,
* received data bytes in master-receiver or slave-receiver mode. ([Microchip][1])

In receiver modes, sample `TWEA` before the ACK bit of the next byte.

### `TWSTA`

Writing `TWSTA=1` requests START. Hardware checks whether the bus is free. If free, it generates START; if busy, it waits for STOP and then generates START. `TWSTA` must be cleared by software after the START has been transmitted. ([Microchip][1])

Model:

```c
if (TWSTA && clear_twint && TWEN) {
    if (bus_free || already_master) schedule_start_or_repeated_start();
    else state = TWI_WAIT_BUS_FREE_FOR_START;
}
```

### `TWSTO`

In master mode, writing `TWSTO=1` generates STOP; after STOP is executed, hardware clears `TWSTO`. In slave mode, setting `TWSTO` does not generate STOP; it recovers to unaddressed slave mode and releases SCL/SDA. ([Microchip][1])

### `TWWC`

`TWWC` is set if software writes `TWDR` while `TWINT` is low. It is cleared by writing `TWDR` while `TWINT` is high. ([Microchip][1])

Emulator behavior:

```c
write_TWDR(value) {
    if (!TWINT) {
        TWWC = 1;
        // Usually do not update TWDR, or update only if hardware test shows so.
        // Datasheet implies the register is inaccessible.
    } else {
        TWDR = value;
        TWWC = 0;
    }
}
```

I would not allow `TWDR` to change when `TWINT=0` unless you later measure hardware and find otherwise.

### `TWEN`

When `TWEN=1`, TWI takes control of SCL/SDA. When written to zero, TWI is switched off and all transmissions terminate. ([Microchip][1])

On `TWEN=0`:

```c
state = TWI_DISABLED;
role = TWI_ROLE_NONE;
drive_scl_low = false;
drive_sda_low = false;
status = 0xF8;
```

Do not necessarily reset all registers; disabling the peripheral is not the same as MCU reset.

---

## `TWSR`

`TWSR[7:3]` is the status code. `TWSR[1:0]` is the prescaler. Bit 2 reads zero. The datasheet notes that software should mask the prescaler bits to zero when checking status. ([Microchip][1])

Emulator behavior:

```c
read_TWSR() {
    return status | (TWSR_prescaler_bits & 0x03);
}
```

When `TWINT=0`, the visible status should normally be `0xF8`, meaning no relevant state information is available. The datasheet says `0xF8` occurs between other states and when TWI is not involved in a serial transfer. ([Microchip][1])

So:

```c
uint8_t visible_status(void) {
    if (!TWINT) return 0xF8;
    return status;
}
```

Then return:

```c
return visible_status() | twps_bits;
```

---

## `TWDR`

In transmit mode, `TWDR` contains the next byte to transmit. In receive mode, it contains the last byte received. It is writable while TWI is not shifting a byte, which occurs when `TWINT` is set. During shifting, data on the bus is simultaneously shifted in, and `TWDR` normally contains the last byte present on the bus, except after wake from sleep by TWI interrupt. ([Microchip][1])

For emulation:

* Latch `TWDR` into `shift_reg` when a byte transmission starts.
* Do not mutate visible `TWDR` bit-by-bit unless you are emulating very obscure firmware that reads `TWDR` while `TWINT=0`.
* On completed receive byte, set `TWDR = received_byte` before setting `TWINT`.

---

## `TWAR` and `TWAMR`

`TWAR[7:1]` is the own slave address. `TWAR[0]`, `TWGCE`, enables recognition of the general-call address `0x00`. `TWAMR[7:1]` masks address bits; if a mask bit is 1, the corresponding `TWAR` bit is ignored during address matching. ([Microchip][1])

Address match function:

```c
bool own_address_matches(uint8_t addr7) {
    uint8_t own  = TWAR >> 1;
    uint8_t mask = TWAMR >> 1;

    return ((addr7 ^ own) & ~mask) == 0;
}

bool general_call_matches(uint8_t addr7, bool rw) {
    return addr7 == 0x00 && rw == 0 && (TWAR & 1);
}
```

General call with read is meaningless on I²C/TWI because multiple slaves could transmit conflicting data; the datasheet says general call followed by read is meaningless. ([Microchip][1])

---

# 6. Bus event detection

Track previous filtered bus levels:

```c
prev_scl, prev_sda
cur_scl, cur_sda
```

Detect:

```c
start_condition = prev_sda == 1 && cur_sda == 0 && cur_scl == 1;
stop_condition  = prev_sda == 0 && cur_sda == 1 && cur_scl == 1;
scl_rising      = prev_scl == 0 && cur_scl == 1;
scl_falling     = prev_scl == 1 && cur_scl == 0;
```

START and STOP are the special cases where SDA changes while SCL is high. During data transfer, SDA is supposed to be stable while SCL is high.

## Bus busy

```c
if (start_condition) bus_busy = true;
if (stop_condition)  bus_busy = false;
```

A repeated START keeps the bus busy.

## Illegal START/STOP → bus error

The datasheet defines status `0x00` as bus error: a START or STOP occurred at an illegal position in the frame, such as during an address byte, data byte, or acknowledge bit. Recovery is done by setting `TWSTO` and clearing `TWINT`; the TWI enters not-addressed slave mode, clears `TWSTO`, releases SDA/SCL, and does not transmit STOP. ([Microchip][1])

So implement:

```c
if ((start_condition || stop_condition) && transfer_bit_index_inside_byte_or_ack()) {
    status = 0x00;
    TWINT = 1;
    state = TWI_BUS_ERROR;
    hold_scl_low_if_applicable();
}
```

---

# 7. Master transmit timing

## 7.1 START generation

When software writes:

```c
TWCR = (1<<TWINT) | (1<<TWSTA) | (1<<TWEN) | ...
```

and the bus is free, generate START:

```text
SCL high
SDA falls high→low
status = 0x08
TWINT = 1
```

If already master and software requests another START, generate repeated START:

```text
status = 0x10
TWINT = 1
```

The datasheet states that after START is transmitted, hardware sets `TWINT` and status becomes `0x08`; after repeated START, status becomes `0x10`. ([Microchip][1])

A practical timing model:

```text
cycle N:     CPU writes TWCR with TWINT=1,TWSTA=1,TWEN=1
cycle N+1:   TWI observes request
cycle N+1..: wait until bus free and SCL/SDA high
then:        drive SDA low while SCL high
after t_HD_STA model delay: set TWINT, status 0x08/0x10
```

For deterministic emulation, choose:

```c
start_delay_cycles = half_period_cycles;
```

That means START consumes one half SCL phase before `TWINT` is set. This is an implementation choice; hardware calibration is needed if firmware polls `TWINT` cycle-by-cycle immediately after requesting START.

## 7.2 Sending address/data byte

After `0x08` or `0x10`, software loads `TWDR` with `SLA+W`, `SLA+R`, or data, then clears `TWINT`.

For master transmitter:

```text
for bits 7 downto 0:
    during SCL low: drive SDA low for 0, release SDA for 1
    release SCL for high phase
    sample SDA during SCL high for arbitration
    pull SCL low for low phase

ACK bit:
    release SDA
    release SCL high
    sample SDA: low = ACK, high = NACK
    pull SCL low
    set TWINT and status
```

One byte including ACK takes:

```text
9 * scl_period_cycles
```

plus any clock stretching delay.

Status after `SLA+W`:

```text
0x18 = SLA+W transmitted, ACK received
0x20 = SLA+W transmitted, NACK received
0x38 = arbitration lost
```

Status after data:

```text
0x28 = data transmitted, ACK received
0x30 = data transmitted, NACK received
0x38 = arbitration lost
```

These are the master-transmitter status codes in the datasheet. ([Microchip][1])

## 7.3 Arbitration during master transmit

During each data/address bit where the master releases SDA for a logical 1, check the actual bus SDA during SCL high:

```c
if (intended_sda == 1 && bus_sda == 0) {
    arbitration_lost = true;
}
```

If arbitration is lost, the datasheet says the losing master releases SDA and switches toward slave behavior to check whether the winning master is addressing it. ([Microchip][1])

Practical rule:

* If arbitration is lost during SLA/address and the received address matches own address, enter slave receiver or slave transmitter and use `0x68`, `0x78`, or `0xB0`.
* If not addressed, set `0x38`.
* If software responds to `0x38` with `TWSTA=1`, wait for bus free and issue START.
* If software responds with `TWSTA=0`, release bus and enter not-addressed slave mode.

The datasheet summarizes arbitration outcomes as `0x38`, `0x68/0x78`, and `0xB0` depending on whether own address/general call is received and direction. ([Microchip][1])

---

# 8. Master receiver timing

After START, software sends `SLA+R`.

Status after `SLA+R`:

```text
0x40 = SLA+R transmitted, ACK received
0x48 = SLA+R transmitted, NACK received
0x38 = arbitration lost in SLA+R or NACK bit
```

Then bytes are received:

```text
for bits 7 downto 0:
    release SDA
    release SCL high
    sample SDA during SCL high
    pull SCL low

ACK bit:
    if TWEA=1 drive SDA low   // ACK
    if TWEA=0 release SDA     // NACK
    clock ninth bit
    set TWDR = received byte
    set status:
        0x50 if ACK returned
        0x58 if NACK returned
    set TWINT
    stretch SCL low
```

The datasheet’s master-receiver table gives `0x40`, `0x48`, `0x50`, and `0x58`, and says `TWEA` determines whether the next received byte is ACKed or NACKed. ([Microchip][1])

Important timing detail: sample `TWEA` when beginning the receive byte or before the ACK bit. Software normally sets/clears `TWEA` while `TWINT=1`; clearing `TWINT` begins the receive operation using the then-current `TWEA`.

---

# 9. STOP and repeated START

## STOP in master mode

When software writes `TWSTO=1` and clears `TWINT`:

```text
ensure SDA low
ensure/release SCL high
SDA rises low→high while SCL high
TWSTO auto-clears
state = idle / not-addressed slave
bus_busy = false
```

The datasheet says writing `TWSTO` in master mode generates STOP, and `TWSTO` is cleared automatically when STOP is executed. ([Microchip][1])

A practical timing model:

```c
stop_delay_cycles = half_period_cycles;
```

Then clear `TWSTO`. Do **not** set `TWINT` just because STOP completed; AVR TWI firmware normally does not wait for `TWINT` after STOP.

## STOP followed by START

The master tables allow software to request `TWSTO=1` and `TWSTA=1` together after certain statuses. The datasheet says this produces STOP followed by START, with `TWSTO` reset. ([Microchip][1])

Model:

```c
if (TWSTO && TWSTA && clear_twint) {
    schedule_stop();
    after STOP complete:
        TWSTO = 0;
        wait bus free;
        schedule_start();
}
```

---

# 10. Slave receiver behavior

The TWI can be addressed as a slave when:

```text
TWEN = 1
TWEA = 1
address matches TWAR/TWAMR
or general call matches and TWGCE = 1
```

On external START:

```c
state = TWI_SLAVE_ADDR_RECV;
shift_reg = 0;
bit_index = 0;
```

On each SCL rising edge, shift in address bits and R/W. On the 9th clock, decide whether to ACK:

```c
if (own_address_matches(addr7) && TWEA) {
    drive SDA low during ACK bit;
    if (rw == 0) status = 0x60; // own SLA+W
    else         status = 0xA8; // own SLA+R, slave transmitter
} else if (general_call_matches(addr7, rw) && TWEA) {
    drive SDA low during ACK bit;
    status = 0x70;
} else {
    release SDA; // not addressed
}
```

After ACKing own `SLA+W`, set `TWINT=1` and stretch SCL low. The slave-receiver table gives `0x60`, `0x68`, `0x70`, and `0x78` for address/general-call reception, including arbitration-lost variants. ([Microchip][1])

For subsequent received data bytes:

```text
0x80 = data received after own SLA+W, ACK returned
0x88 = data received after own SLA+W, NACK returned
0x90 = data received after general call, ACK returned
0x98 = data received after general call, NACK returned
0xA0 = STOP or repeated START received while still addressed
```

The datasheet lists these slave-receiver states and the transition back to not-addressed slave mode after `0x88`, `0x98`, or `0xA0`. ([Microchip][1])

## Slave receiver ACK timing

After `TWINT` is cleared, the next byte is received. If `TWEA=1`, ACK the byte; if `TWEA=0`, NACK the byte and transition to not-addressed slave after the byte.

The datasheet says if `TWEA` is reset during a transfer, the TWI returns NACK after the next received data byte, and while `TWEA=0` it does not acknowledge its own address, though it still monitors the bus. ([Microchip][1])

---

# 11. Slave transmitter behavior

After ACKing own `SLA+R`, the TWI enters slave transmitter mode:

```text
0xA8 = own SLA+R received, ACK returned
0xB0 = arbitration lost as master; own SLA+R received, ACK returned
```

Software must load `TWDR` and clear `TWINT`.

Then for each byte:

```text
for bits 7 downto 0:
    during SCL low: drive SDA for bit 0, release for bit 1
    on SCL high: allow master to sample
ACK bit:
    release SDA
    on SCL high: sample master's ACK/NACK
```

Statuses:

```text
0xB8 = data transmitted, ACK received
0xC0 = data transmitted, NACK received
0xC8 = last data byte transmitted with TWEA=0, ACK received
```

The slave-transmitter table lists these statuses and describes the `TWEA=0` final-byte behavior. ([Microchip][1])

If `TWEA=0`, the current byte is treated as the last byte. If the master ACKs anyway and asks for more data, the 32U4 enters `0xC8`; after that, the slave is switched to not-addressed mode and, if the master keeps reading, it receives all 1s because the slave releases SDA. The datasheet describes this exact `0xC8` case. ([Microchip][1])

---

# 12. SCL stretching

There are two forms to model.

## 12.1 External slave stretches SCL

When the 32U4 is master and releases SCL high, the bus may remain low because another device is holding it.

Model:

```c
release_scl_for_high_phase();
while (bus_scl == 0) {
    // do not advance high-phase timer
    wait one CPU cycle;
}
start_high_phase_counter();
```

The datasheet explicitly says a slave can extend the SCL low period by pulling SCL low, and this does not affect the SCL high period determined by the master. ([Microchip][1])

## 12.2 32U4 stretches SCL while `TWINT=1`

When the TWI needs software response:

```c
TWINT = 1;
state = TWI_HALTED_TWINT;
drive_scl_low = true;
```

Release SCL only when software clears `TWINT` and the next operation is scheduled.

This is very important for cycle accuracy because firmware may run an ISR or poll loop while the bus is physically held.

---

# 13. Interrupt behavior

The TWI interrupt request is active while:

```c
TWINT == 1 && TWIE == 1 && SREG.I == 1
```

The datasheet says `TWIE` plus global interrupt enable causes the TWI interrupt request to be activated for as long as `TWINT` is high. ([Microchip][1])

So your interrupt controller should see it as level-sensitive, not edge-only.

Pseudo-model:

```c
if (TWCR & TWIE && TWCR & TWINT && SREG_I) {
    request_interrupt(TWI_VECTOR);
}
```

Entering the ISR does **not** clear `TWINT`.

---

# 14. Recommended per-cycle update order

A stable emulator update order:

```text
1. CPU executes current cycle / completes instruction micro-step.
2. CPU memory write side effects are applied to TWI registers.
3. TWI internal scheduled countdown advances by one CPU cycle.
4. TWI updates its desired SCL/SDA drive-low outputs.
5. Global bus resolver computes physical SCL/SDA.
6. Each TWI instance samples filtered SCL/SDA.
7. START/STOP/SCL edges are detected.
8. TWI state machines consume detected edges.
9. TWINT/status/interrupt request are updated.
10. GPIO input reads for next CPU cycle see resolved pin levels.
```

The exact ordering between CPU writes and peripheral sampling can matter for pathological tests. AVR documentation does not fully specify the hidden TWI pipeline, so pick one global ordering and use it consistently. If you later test hardware, adjust only the write-to-action and action-to-`TWINT` offsets.

---

# 15. Status-code implementation table

Use this as your state-machine target.

## Master transmitter

| Status | Meaning                    | Next hardware behavior after software clears `TWINT`  |
| -----: | -------------------------- | ----------------------------------------------------- |
| `0x08` | START transmitted          | Send `TWDR` as `SLA+W` or `SLA+R`                     |
| `0x10` | Repeated START transmitted | Send `TWDR` as `SLA+W` or `SLA+R`                     |
| `0x18` | `SLA+W` ACKed              | Send data, repeated START, STOP, or STOP+START        |
| `0x20` | `SLA+W` NACKed             | Send data anyway, repeated START, STOP, or STOP+START |
| `0x28` | Data ACKed                 | Send next data, repeated START, STOP, or STOP+START   |
| `0x30` | Data NACKed                | Send next data, repeated START, STOP, or STOP+START   |
| `0x38` | Arbitration lost           | Release bus or wait for bus free and START            |

## Master receiver

| Status | Meaning                                 |
| -----: | --------------------------------------- |
| `0x08` | START transmitted                       |
| `0x10` | Repeated START transmitted              |
| `0x38` | Arbitration lost in `SLA+R` or NACK bit |
| `0x40` | `SLA+R` ACKed                           |
| `0x48` | `SLA+R` NACKed                          |
| `0x50` | Data received, ACK returned             |
| `0x58` | Data received, NACK returned            |

## Slave receiver

| Status | Meaning                                           |
| -----: | ------------------------------------------------- |
| `0x60` | Own `SLA+W` received, ACK returned                |
| `0x68` | Arbitration lost as master; own `SLA+W` received  |
| `0x70` | General call received, ACK returned               |
| `0x78` | Arbitration lost as master; general call received |
| `0x80` | Data after own `SLA+W`, ACK returned              |
| `0x88` | Data after own `SLA+W`, NACK returned             |
| `0x90` | Data after general call, ACK returned             |
| `0x98` | Data after general call, NACK returned            |
| `0xA0` | STOP or repeated START while addressed            |

## Slave transmitter

| Status | Meaning                                           |
| -----: | ------------------------------------------------- |
| `0xA8` | Own `SLA+R` received, ACK returned                |
| `0xB0` | Arbitration lost as master; own `SLA+R` received  |
| `0xB8` | Data transmitted, ACK received                    |
| `0xC0` | Data transmitted, NACK received                   |
| `0xC8` | Last data transmitted with `TWEA=0`, ACK received |

## Miscellaneous

| Status | Meaning                                      |
| -----: | -------------------------------------------- |
| `0xF8` | No relevant state info; `TWINT=0`            |
| `0x00` | Bus error due to illegal START/STOP position |

---

# 16. Minimal master byte-transfer scheduler

A useful internal scheduler for master transmit:

```c
void twi_begin_master_send_byte(uint8_t byte) {
    shift_reg = byte;
    bit_index = 0;
    state = TWI_MASTER_SEND_BYTE;
    scl_phase_high = false;

    // Begin with SCL low and data valid.
    drive_scl_low = true;
    drive_sda_low = ((shift_reg & 0x80) == 0);
    phase_countdown = half_period_cycles;
}

void twi_tick_master_send_byte(void) {
    if (phase_countdown > 0) {
        phase_countdown--;
        return;
    }

    if (!scl_phase_high) {
        // Move to high phase: release SCL.
        drive_scl_low = false;

        // Wait for external stretch.
        if (!bus_scl) return;

        scl_phase_high = true;
        phase_countdown = half_period_cycles;

        // Arbitration check while high.
        bool intended_sda_high = !drive_sda_low;
        if (intended_sda_high && !bus_sda) {
            arbitration_lost = true;
        }

        return;
    }

    // End high phase: pull SCL low.
    drive_scl_low = true;
    scl_phase_high = false;

    if (bit_index < 7) {
        bit_index++;
        uint8_t mask = 0x80 >> bit_index;
        drive_sda_low = ((shift_reg & mask) == 0);
        phase_countdown = half_period_cycles;
    } else if (bit_index == 7) {
        // ACK bit: transmitter releases SDA.
        bit_index = 8;
        drive_sda_low = false;
        phase_countdown = half_period_cycles;
    } else {
        // ACK bit high phase has completed; sample ACK.
        bool ack = !sampled_sda_during_ack_high;
        finish_master_transmit_byte(ack, arbitration_lost);
    }
}
```

The actual implementation should sample ACK while SCL is high, not after pulling SCL low; the above is schematic.

---

# 17. Minimal slave receiver edge-driven model

Slave mode should be edge-driven by external SCL:

```c
void twi_on_scl_rising_slave_rx(void) {
    if (receiving_8_bits) {
        shift_reg = (shift_reg << 1) | bus_sda;
        bit_index++;

        if (bit_index == 8) {
            decide_ack_for_next_ack_bit();
        }
    } else if (ack_bit_phase) {
        // Master has clocked our ACK/NACK.
        drive_sda_low = false;

        if (address_byte_complete) {
            enter_addressed_state_and_set_twint();
        } else {
            TWDR = shift_reg;
            set_status_data_received();
            TWINT = 1;
            drive_scl_low = true; // stretch
        }
    }
}

void twi_on_scl_falling_slave_rx(void) {
    if (next_phase_is_ack) {
        drive_sda_low = should_ack;
    }
}
```

Use falling edge to prepare SDA for the next bit; use rising edge to sample.

---

# 18. Sleep behavior

For sleep modes other than Idle, the datasheet says the TWI clock system is off, but if `TWEA=1`, the interface can still acknowledge own address or general call using the TWI bus clock, wake the MCU, and hold SCL low during wake-up and until `TWINT` is cleared. It also says `TWDR` does not reflect the last byte on the bus when waking from these sleep modes. ([Microchip][1])

For emulator purposes:

```c
if (sleeping_non_idle && TWEN && TWEA && address_match_seen) {
    drive_ack_low_on_9th_clock();
    wake_cpu_from_sleep();
    TWINT = 1;
    status = matched_status; // 0x60, 0x70, or 0xA8
    TWDR = undefined_or_preserved_garbage;
    drive_scl_low = true;
}
```

---

# 19. What not to overfit unless you test hardware

The public datasheet is enough for a very accurate behavioral model, but not enough to guarantee these hidden details:

* Exact number of CPU cycles from `TWCR` write to first SDA/SCL transition.
* Exact cycle on which `TWINT` becomes readable after the final ACK clock.
* Exact START/STOP setup/hold phase in CPU cycles.
* Exact behavior of `TWDR` writes during `TWINT=0` beyond `TWWC` being set.
* Exact low/high duty split inside the SCL period.
* Analog pull-up rise time unless you model RC bus physics.
* Sub-50 ns spike behavior unless your simulation has sub-cycle resolution.
* The undefined `TWBR < 10` master-mode quirk.

For most emulator software compatibility, the biggest wins are:

1. Correct status codes.
2. Correct `TWINT`/SCL-stretch behavior.
3. Correct open-drain pin resolution.
4. Correct byte timing from `TWBR/TWPS`.
5. Correct slave address-match and ACK timing.
6. Correct START/STOP/repeated-START detection.
7. Correct arbitration loss behavior.

A strong practical model is: **byte transfer = 9 SCL periods; SCL period = `16 + 2*TWBR*prescaler` CPU cycles; status/TWINT occur at the end of the 9th clock’s falling edge; while `TWINT=1`, SCL is held low until software clears it.** That is close enough for most firmware and gives you clean places to insert hardware-calibrated offsets later.

[1]: https://ww1.microchip.com/downloads/en/devicedoc/atmel-7766-8-bit-avr-atmega16u4-32u4_datasheet.pdf "ATmega16U4/32U4 Datasheet"
