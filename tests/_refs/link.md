A good architecture is to **separate local cycle-accurate pin simulation from remote I²C transaction transport**.

Do **not** try to stream every SCL/SDA edge over the network. That only works if both emulators run in one deterministic lockstep simulation. For variable-latency networked play, make each CPU see a **local open-drain I²C bus**, and attach a **link adapter** to that bus. The adapter translates local I²C bus activity into ordered abstract I²C transactions, then uses clock stretching to hide remote latency.

## 1. Recommended architecture

```text
             Local cycle-accurate domain A
        ┌─────────────────────────────────────┐
        │                                     │
        │   ATmega32U4 A                      │
        │   ┌──────────────┐                  │
        │   │TWI peripheral│                  │
        │   └─────┬───┬────┘                  │
        │         │   │                       │
        │       SDA  SCL                      │
        │         │   │                       │
        │   ┌─────┴───┴─────┐                 │
        │   │ Open-drain    │                 │
        │   │ local I²C bus │                 │
        │   └─────┬───┬─────┘                 │
        │         │   │                       │
        │   ┌─────┴───┴─────┐                 │
        │   │Link Adapter A │                 │
        │   └───────┬───────┘                 │
        └───────────│─────────────────────────┘
                    │
             Ordered transaction transport
                    │
        ┌───────────│─────────────────────────┐
        │   ┌───────┴───────┐                 │
        │   │Link Adapter B │                 │
        │   └─────┬───┬─────┘                 │
        │         │   │                       │
        │   ┌─────┴───┴─────┐                 │
        │   │ Open-drain    │                 │
        │   │ local I²C bus │                 │
        │   └─────┬───┬─────┘                 │
        │         │   │                       │
        │       SDA  SCL                      │
        │         │   │                       │
        │   ┌─────┴───┴────┐                  │
        │   │TWI peripheral│                  │
        │   └──────────────┘                  │
        │   ATmega32U4 B                      │
        │                                     │
        └─────────────────────────────────────┘
             Local cycle-accurate domain B
```

Each 32U4 emulator knows only this:

```text
my TWI pins are connected to an open-drain bus
```

It does **not** know whether the other side is:

```text
another 32U4,
a display controller,
a sensor,
a debug stub,
a replay file,
or a network peer.
```

That knowledge lives outside the CPU emulator, in the link/device layer.

---

# 2. Key principle: local wires, remote transactions

The 32U4 TWI peripheral should remain a **pin-level peripheral**. It drives and samples local `SDA` and `SCL` every emulated CPU cycle.

The link adapter is an **external bus participant**. It also connects to local `SDA` and `SCL`, observes I²C traffic, and can pull either line low.

The adapter converts this:

```text
START
address byte
ACK/NACK
data byte
ACK/NACK
...
STOP
```

into this:

```text
BeginTransaction(address, read/write)
WriteByte(byte) -> ACK/NACK
ReadByte() -> byte
MasterAck(ACK/NACK)
RepeatedStart(...)
Stop()
```

Network latency is hidden by having the adapter hold `SCL` low. From the local 32U4’s perspective, this is just normal I²C clock stretching.

That is the central trick.

---

# 3. Why not tunnel raw SCL/SDA over the network?

You can support two modes:

## Mode A: true shared-wire mode

Use this when both emulated CPUs are in the same process, or in a deterministic lockstep simulator.

```text
32U4 A TWI ─┐
            ├── same OpenDrainBus object
32U4 B TWI ─┘
```

This gives the most accurate behavior. Both TWI peripherals see the same SCL/SDA transitions on the same emulated cycle.

This is ideal for:

```text
same-process multiplayer,
testbenches,
hardware-accurate multi-master arbitration,
bit-banged protocols,
cycle-level validation.
```

## Mode B: network bridge mode

Use this when the remote side may have variable latency.

```text
32U4 A TWI ─ local bus ─ Link Adapter A
                              │
                         transaction stream
                              │
32U4 B TWI ─ local bus ─ Link Adapter B
```

This is not a literal shared wire. It is a **semantically equivalent I²C transaction bridge** with clock stretching. It preserves local cycle accuracy and most software-visible TWI behavior, but it does not make the two emulators share the exact same SCL/SDA edge timeline.

For network play, this is the practical architecture.

---

# 4. Local bus abstraction

The CPU’s TWI peripheral should only talk to a generic open-drain bus.

```c
typedef struct {
    bool drive_sda_low;
    bool drive_scl_low;
} I2CBusDrive;

typedef struct {
    bool sda;
    bool scl;
} I2CBusSample;
```

Each bus participant contributes:

```c
participant.drive_sda_low
participant.drive_scl_low
```

The bus resolves levels as:

```c
bus.sda = !any_participant_drives_sda_low;
bus.scl = !any_participant_drives_scl_low;
```

The 32U4 TWI peripheral is just one participant. The link adapter is another.

The CPU-side TWI model should not call network code. It should not know about packets, peers, sockets, or remote devices.

---

# 5. Link adapter responsibilities

Each link adapter has two halves.

## 5.1 Local bus front-end

This connects to the local open-drain bus.

It does the following:

```text
observes START/STOP
captures address bytes
captures write data bytes
generates ACK/NACK
supplies read data bytes
performs clock stretching
detects repeated START
optionally induces arbitration loss
```

It must behave like an I²C participant on the local bus.

When the local CPU is master, the adapter behaves like a remote slave proxy.

When a remote peer wants to initiate a transaction into this CPU, the adapter behaves like a local I²C master.

## 5.2 Remote transaction back-end

This connects to some abstract remote endpoint.

```c
struct I2CRemoteEndpoint {
    Future<Ack> begin(uint8_t address7, I2CDirection dir);
    Future<Ack> write_byte(uint8_t byte);
    Future<uint8_t> read_byte();
    Future<void> master_ack(bool ack);
    Future<Ack> repeated_start(uint8_t address7, I2CDirection dir);
    Future<void> stop();
};
```

Different implementations can exist:

```text
Remote32U4BusPeer
I2CDisplayModel
I2CSensorModel
ReplayFileDevice
DebugConsoleDevice
NetworkPeerTransport
```

The 32U4 emulator should not care which one is attached.

---

# 6. Outgoing transaction: CPU A writes to remote CPU B

Suppose CPU A is master and CPU B is slave.

From CPU A’s point of view:

```text
START
SLA+W
DATA0
DATA1
STOP
```

Internally, the bridge does this:

```text
A CPU generates START locally.
A Link Adapter observes START.

A CPU sends SLA+W.
A Link Adapter captures address.
A Link Adapter holds SCL low before ACK.
A Link Adapter sends Begin(address, Write) to B side.

B Link Adapter becomes a local master on B's bus.
B Link Adapter generates START + SLA+W to CPU B.
CPU B's TWI decides whether to ACK.
B Link Adapter reports ACK/NACK back to A Link Adapter.

A Link Adapter releases/ACKs the address phase.
A CPU sees normal 0x18 or 0x20 TWI status.
```

For each data byte:

```text
A CPU clocks out data byte.
A Link Adapter captures byte.
A Link Adapter stretches SCL before ACK.

B Link Adapter writes that byte to CPU B as a local I²C master.
CPU B ACKs or NACKs.
B Link Adapter reports ACK/NACK.

A Link Adapter returns the same ACK/NACK to CPU A.
A CPU sees 0x28 or 0x30.
```

On STOP:

```text
A CPU generates STOP.
A Link Adapter observes STOP.
A Link Adapter sends Stop() to B Link Adapter.
B Link Adapter generates STOP on B's local bus.
CPU B sees STOP / repeated START as normal TWI slave behavior.
```

The remote latency appears to CPU A as a long slave clock-stretch. CPU A continues executing instructions, but its TWI transfer does not complete until the adapter releases SCL.

---

# 7. Outgoing read: CPU A reads from remote CPU B

This is a little trickier because the master’s ACK/NACK after each byte must be forwarded to the remote slave.

Flow:

```text
A CPU: START + SLA+R
A Adapter: capture address, stretch before ACK
B Adapter: generate START + SLA+R on B bus
B CPU: ACKs if addressed as slave transmitter
B Adapter: reports ACK
A Adapter: ACKs SLA+R to A CPU
```

For each read byte:

```text
B CPU provides byte to B Adapter.
B Adapter sends byte to A Adapter.
A Adapter stretches A's SCL until byte is available.
A Adapter presents byte bit-by-bit to A CPU.
A CPU returns ACK or NACK.
A Adapter forwards that ACK/NACK to B Adapter.
B Adapter clocks the same ACK/NACK into B CPU.
B CPU receives 0xB8, 0xC0, or 0xC8 as appropriate.
```

This preserves the important thing: both CPUs see normal TWI status transitions locally.

---

# 8. Clock stretching as the latency buffer

The adapter should stretch SCL in these places:

## During address ACK

```text
local master has sent 8 address bits
adapter needs remote ACK/NACK
adapter holds SCL low before 9th clock
remote decision arrives
adapter drives SDA low for ACK or releases SDA for NACK
adapter releases SCL
```

## During write-byte ACK

```text
local master has sent 8 data bits
adapter holds SCL low before ACK bit
remote write completes
adapter ACKs/NACKs
```

## Before read-byte data

```text
local master is ready to receive next byte
adapter does not yet have remote byte
adapter holds SCL low
remote byte arrives
adapter clocks byte out locally
```

## Before forwarding master ACK in read mode

```text
local master receives byte and returns ACK/NACK
adapter forwards that ACK/NACK to remote side
remote side may need to complete its own local byte phase
```

This lets variable network latency become legal I²C behavior.

---

# 9. Generic transaction protocol

Use an ordered message stream. Something like:

```c
enum I2CMsgKind {
    I2C_MSG_START,
    I2C_MSG_ADDRESS,
    I2C_MSG_WRITE_BYTE,
    I2C_MSG_READ_BYTE_REQUEST,
    I2C_MSG_READ_BYTE_RESPONSE,
    I2C_MSG_MASTER_ACK,
    I2C_MSG_REPEATED_START,
    I2C_MSG_STOP,
    I2C_MSG_ABORT,
    I2C_MSG_BUS_RESET
};

struct I2CMsg {
    uint64_t link_sequence;
    uint32_t transaction_id;
    I2CMsgKind kind;

    union {
        struct {
            uint8_t address7;
            bool read;
        } address;

        struct {
            uint8_t byte;
        } write_byte;

        struct {
            uint8_t byte;
        } read_byte_response;

        struct {
            bool ack;
        } ack;
    };
};
```

The important properties are:

```text
reliable ordering,
transaction IDs,
explicit repeated START,
explicit STOP,
flow control,
timeout/abort handling,
link reset handling.
```

For network transport, TCP/WebSocket/QUIC-style reliable ordering is much easier than unordered datagrams. I²C is strictly ordered, so an ordered stream is natural.

---

# 10. Address handling

The local link adapter should not need to know the remote device type.

When it sees an address phase:

```text
SLA+W or SLA+R
```

it asks the remote endpoint:

```c
Ack ack = remote.begin(address7, direction);
```

If the remote endpoint is a display model, it may immediately return ACK/NACK.

If the remote endpoint is another cycle-accurate 32U4 bus, the remote adapter physically performs the address phase on that CPU’s local bus and reports whether anything ACKed.

This means the remote 32U4 may freely change:

```text
TWAR
TWAMR
TWEA
TWGCE
TWEN
PRTWI
sleep state
```

and the other CPU does not need to know. The only observable result is whether the address phase ACKs.

---

# 11. Keeping CPU emulation cycle-accurate

Each CPU emulator should continue ticking independently.

When a local CPU waits on remote I²C latency, do **not** pause the whole CPU emulator. Instead:

```text
TWI transfer remains incomplete.
SCL is held low by the link adapter.
CPU continues executing instructions.
Firmware polling TWINT keeps seeing TWINT = 0.
Firmware polling SCL sees SCL = 0.
When remote response arrives, SCL is released and the TWI state machine completes.
```

That matches physical I²C clock stretching much better than suspending the CPU emulator.

For example, a typical master transmit poll loop:

```c
TWCR = (1 << TWINT) | (1 << TWEN);
while (!(TWCR & (1 << TWINT))) {
    // CPU keeps running here
}
```

With network latency, this loop simply runs longer because the local adapter stretches SCL.

---

# 12. Multi-master support

This is the hardest part.

If both CPUs can initiate I²C transactions at the same time, literal bit-level arbitration across a variable-latency network is not realistically cycle-accurate unless both sides run in lockstep shared-wire mode.

For network bridge mode, use a **link-level bus lease**.

```text
START request asks for link ownership.
One side wins.
Repeated START keeps ownership.
STOP releases ownership.
```

A simple deterministic rule:

```text
if both sides request START concurrently:
    lower endpoint ID wins
    loser waits or receives synthetic arbitration loss
```

You have two design choices for the loser.

## Simpler option: wait

The losing adapter simply does not complete the local transaction yet. It holds SCL low until the remote transaction finishes.

Effectively:

```text
local master experiences very long clock stretching
```

This is robust, but not a perfect model of I²C multi-master arbitration.

## More accurate option: synthesize arbitration loss

The losing adapter forces the local master to lose arbitration during the address byte by pulling SDA low at a point where the local master attempted to release SDA high.

Then the local 32U4 TWI reports:

```text
0x38 = arbitration lost
```

This is closer to real I²C, but it is more complicated and has edge cases, especially if the losing byte contains few or no 1 bits.

My recommendation:

```text
Implement wait-based arbitration first.
Add synthetic arbitration loss later as an optional accuracy mode.
Use true shared-wire lockstep mode for tests that require exact multi-master arbitration.
```

For most link-cable uses, especially if software naturally has one side acting as master and the other as slave, the bus-lease model is sufficient.

---

# 13. Remote CPU as an abstract I²C target

When the remote endpoint is another 32U4, do not make CPU A talk directly to CPU B’s TWI internals.

Instead, adapter B should act as a normal local I²C master on CPU B’s bus.

```text
CPU A master
   ↓ local bus A
Adapter A target proxy
   ↓ transaction transport
Adapter B master injector
   ↓ local bus B
CPU B slave
```

And in the reverse direction:

```text
CPU B master
   ↓ local bus B
Adapter B target proxy
   ↓ transaction transport
Adapter A master injector
   ↓ local bus A
CPU A slave
```

This keeps the abstraction clean. A 32U4 sees only electrical bus behavior. It does not know whether the initiating master is another AVR, a display driver test harness, or a synthetic tool.

---

# 14. Generic backend examples

## Another emulated 32U4

```c
class RemoteCpuI2CEndpoint : public I2CRemoteEndpoint {
    LocalI2CMasterInjector injector;

    Future<Ack> begin(uint8_t addr, Direction dir) {
        return injector.start_and_address(addr, dir);
    }

    Future<Ack> write_byte(uint8_t b) {
        return injector.write_byte(b);
    }

    Future<uint8_t> read_byte() {
        return injector.read_byte();
    }

    Future<void> master_ack(bool ack) {
        return injector.clock_master_ack(ack);
    }

    Future<void> stop() {
        return injector.stop();
    }
};
```

## Display controller

```c
class SSD1306Endpoint : public I2CRemoteEndpoint {
    bool selected = false;
    uint8_t address = 0x3C;

    Future<Ack> begin(uint8_t addr, Direction dir) {
        selected = (addr == address && dir == Write);
        return selected ? Ack : Nack;
    }

    Future<Ack> write_byte(uint8_t b) {
        if (!selected) return Nack;
        consume_display_byte(b);
        return Ack;
    }

    Future<void> stop() {
        selected = false;
    }
};
```

CPU-side code does not change.

---

# 15. Handling physical pin polling

Some firmware may read SDA/SCL pins directly.

That is why the local open-drain bus still matters.

The CPU should see:

```text
SCL low while remote latency is pending
SDA low during ACK
SDA changing during data
START/STOP edges locally generated
```

So code that polls SCL for clock stretching or checks bus busy can still work.

However, there is a limitation: if firmware is doing fully custom bit-banged I²C and ignores SCL stretching, network bridge mode may not preserve behavior. For arbitrary bit-banging, use same-process shared-wire mode or a stricter distributed lockstep simulation.

---

# 16. Suggested module boundaries

A clean design might look like this:

```text
AvrCpu
  └── AvrTwiPeripheral
        └── I2CBusPort

I2CBus
  ├── resolves open-drain SDA/SCL
  ├── tracks START/STOP
  └── exposes pin levels to GPIO/TWI

I2CLinkAdapter
  ├── local bus front-end
  ├── I²C decoder/encoder
  ├── clock-stretch controller
  ├── transaction state machine
  └── remote endpoint client/server

I2CRemoteEndpoint
  ├── RemoteCpuBusEndpoint
  ├── DisplayEndpoint
  ├── SensorEndpoint
  └── NetworkEndpoint

I2CTransport
  ├── ordered packets
  ├── sequence numbers
  ├── transaction IDs
  ├── timeouts
  └── reconnect/reset handling
```

The 32U4 emulator should depend only on:

```text
I2CBus
```

not on:

```text
I2CTransport
NetworkEndpoint
RemoteCpuBusEndpoint
DisplayEndpoint
```

---

# 17. State machine inside the link adapter

For the local target-proxy side:

```c
enum LinkProxyState {
    IDLE,

    OBSERVING_ADDRESS,
    WAIT_REMOTE_ADDRESS_ACK,

    LOCAL_WRITE_RECEIVING_BYTE,
    WAIT_REMOTE_WRITE_ACK,

    WAIT_REMOTE_READ_BYTE,
    LOCAL_READ_SENDING_BYTE,
    WAIT_LOCAL_MASTER_ACK,

    REPEATED_START_PENDING,
    STOP_PENDING,
    ERROR
};
```

Typical write byte behavior:

```c
case LOCAL_WRITE_RECEIVING_BYTE:
    if (received_8_bits_from_local_master()) {
        drive_scl_low = true;          // stretch before ACK
        send_remote_write_byte(byte);
        state = WAIT_REMOTE_WRITE_ACK;
    }
    break;

case WAIT_REMOTE_WRITE_ACK:
    if (remote_ack_ready()) {
        drive_sda_low = remote_ack ? true : false;
        release_scl_for_ack_clock();
        state = LOCAL_WRITE_RECEIVING_BYTE;
    }
    break;
```

Typical read byte behavior:

```c
case WAIT_REMOTE_READ_BYTE:
    drive_scl_low = true;              // stretch before data byte
    if (remote_byte_ready()) {
        tx_byte = remote_byte;
        prepare_to_clock_byte_to_local_master(tx_byte);
        state = LOCAL_READ_SENDING_BYTE;
    }
    break;

case LOCAL_READ_SENDING_BYTE:
    clock_bits_to_local_master();
    if (local_master_ack_seen()) {
        send_remote_master_ack(local_ack);
        if (local_ack)
            state = WAIT_REMOTE_READ_BYTE;
        else
            state = IDLE_OR_WAIT_STOP;
    }
    break;
```

---

# 18. Error handling

You need explicit behavior for bad links.

Possible policies:

## Remote timeout during address phase

Options:

```text
stretch SCL forever
eventually NACK
simulate stuck bus
drop link and raise emulator-level disconnect
```

For gameplay, “eventually NACK” may be nicer. For hardware accuracy, “stretch forever” is closer to a hung I²C peripheral.

## Remote timeout during data phase

Options:

```text
hold SCL low
NACK the byte
abort transaction
force bus error
```

I would make this configurable:

```text
Strict mode: hold SCL low indefinitely.
Friendly mode: NACK after timeout.
Debug mode: stop emulation with diagnostic.
```

## Link reset

Both adapters should support:

```text
clear active transaction
release SDA/SCL
drop bus lease
return to idle
optionally notify local bus as device removal
```

Be careful: real I²C has no out-of-band reset. Releasing both lines may leave the local CPU thinking the transfer is still in progress until its own timeout logic runs.

---

# 19. Repeated START handling

Repeated START must be represented explicitly. Do not translate it into STOP+START unless the remote endpoint permits that.

For example:

```text
START
SLA+W
register index
REPEATED START
SLA+R
read data
STOP
```

Many I²C devices depend on repeated START preserving internal transaction context.

So your protocol should distinguish:

```text
RepeatedStart(address, direction)
```

from:

```text
Stop()
Start(address, direction)
```

For the remote-CPU endpoint, adapter B should generate a real repeated START on B’s local bus if it currently holds the local master transaction.

---

# 20. Timing model summary

Within each local emulator:

```text
TWI peripheral remains cycle-accurate.
SDA/SCL are resolved every emulated cycle.
The link adapter participates in the local bus every emulated cycle.
Network latency appears only as SCL stretching.
```

Across the network:

```text
Transactions are ordered.
Latency is variable.
No exact shared cycle timeline is assumed.
Remote edges are not streamed.
```

This gives you a practical hybrid:

```text
cycle-accurate local electrical behavior
+
transaction-accurate remote I²C behavior
+
legal I²C latency absorption via clock stretching
```

---

# 21. Recommended final design

I would implement three connection types behind the same bus API:

```text
1. LocalSharedI2CBus
   True pin-level shared bus.
   Best for same-process two-CPU emulation.

2. RemoteI2CTransactionBridge
   Network-capable bridge.
   Uses local clock stretching and ordered I²C transaction messages.

3. HighLevelI2CDeviceAdapter
   Direct model for displays, sensors, EEPROMs, etc.
   Same abstract endpoint interface as the network bridge.
```

Then configuration decides what is attached:

```text
Arduboy A TWI bus:
    - local pullups
    - RemoteI2CTransactionBridge

Arduboy B TWI bus:
    - local pullups
    - RemoteI2CTransactionBridge
```

or:

```text
Arduboy TWI bus:
    - local pullups
    - SSD1306DisplayEndpoint
```

or:

```text
Testbench:
    - 32U4 A TWI
    - 32U4 B TWI
    - one LocalSharedI2CBus
```

That gives you a clean emulator architecture: the 32U4 remains hardware-accurate, the cable remains external, and the remote side remains generic.
