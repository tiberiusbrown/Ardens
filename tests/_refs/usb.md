Below is an emulator-oriented overview of the **ATmega16U4/ATmega32U4 USB device peripheral**. I’ll focus on the 32U4, but the 16U4 is essentially the same peripheral with less memory elsewhere in the MCU.

The most important framing point: **the 32U4 USB block is a USB device controller, not a host/OTG controller**. It supports USB 2.0 **full-speed** and **low-speed** device operation, with endpoint FIFOs in an independent 832-byte USB DPRAM. Endpoint 0 is the default control endpoint, and endpoints 1–6 are programmable. The datasheet gives good register and event semantics, but it does **not** fully specify every internal clock-cycle latency, so a truly silicon-identical “cycle accurate” emulator will need deterministic assumptions or empirical calibration for some hidden latencies. The datasheet does specify that the USB block uses a 48 MHz USB clock for full-speed operation, derives 12 Mbit/s full-speed or 1.5 Mbit/s low-speed signaling, and uses an internal DPLL for clock recovery. ([Microchip][1])

---

## 1. High-level hardware model

A useful emulator decomposition is:

```text
USB pins / bus model
  VBUS, D+, D-, pull-up selection, detach/attach, SE0/J/K states

USB clock / power domain
  regulator, PLL, FRZCLK, USBE, VBUS pad enable, suspend behavior

USB protocol engine
  reset detection, SOF, suspend, resume, address filtering,
  token/data/handshake processing

Endpoint controller array
  endpoint 0..6 state machines
  endpoint configuration, type, direction, bank count, size
  FIFO/DPRAM allocation and bank state

CPU register interface
  memory-mapped registers, selected endpoint window,
  interrupts, flags, byte counters, UEDATX FIFO access
```

The peripheral’s state is partly **global** and partly **selected-endpoint windowed**. Most endpoint registers — `UECONX`, `UECFG0X`, `UECFG1X`, `UESTA0X`, `UESTA1X`, `UEINTX`, `UEIENX`, `UEDATX`, `UEBCLX`, `UEBCHX` — refer to whichever endpoint number is currently selected in `UENUM`. The CPU must set `UENUM.EPNUM` before accessing a specific endpoint’s register set. ([Microchip][1])

---

## 2. Capabilities and endpoint resources

The 32U4 USB peripheral supports:

| Feature             | 32U4 behavior                                                   |
| ------------------- | --------------------------------------------------------------- |
| USB role            | Device only                                                     |
| Speeds              | Full-speed 12 Mbit/s, low-speed 1.5 Mbit/s                      |
| Endpoint 0          | Default control endpoint, up to 64 bytes                        |
| Endpoint 1          | Up to 256 bytes, one or two banks                               |
| Endpoints 2–6       | Up to 64 bytes, one or two banks                                |
| Transfer types      | Control, bulk, interrupt, isochronous                           |
| DPRAM               | 832 bytes, independent from CPU SRAM                            |
| Interrupts          | One general USB interrupt vector, one endpoint interrupt vector |
| Remote wake-up      | Supported through `RMWKUP`                                      |
| Bus reset CPU reset | Optional through `UDCON.RSTCPU`                                 |

The datasheet describes endpoint 0 as the default control endpoint and endpoints 1–6 as programmable endpoints. Endpoint 1 is the only non-control endpoint with a maximum 256-byte FIFO; endpoints 2–6 top out at 64 bytes. The device supports ping-pong, meaning double-bank operation. ([Microchip][1])

---

## 3. Clocking, PLL, and power domain

The USB block needs a **48 MHz USB clock** for full-speed operation. The 32U4 has an internal PLL that takes a nominal 8 MHz input and can generate 32–96 MHz. For the common 16 MHz crystal configuration, `PLLCSR.PINDIV` is set so the PLL input is divided by 2 to 8 MHz. The PLL is enabled with `PLLCSR.PLLE`, and `PLLCSR.PLOCK` indicates lock after a delay described only as “several ms.” ([Microchip][1])

Relevant PLL registers:

| Register |                 Address | Bits                                      | Emulator notes                                                                                                                                             |
| -------- | ----------------------: | ----------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `PLLCSR` | I/O `0x29`, data `0x49` | `PINDIV`, `PLLE`, `PLOCK`                 | `PINDIV=0` for 8 MHz input, `PINDIV=1` for 16 MHz input. `PLLE` starts PLL. `PLOCK` becomes 1 after PLL lock delay; clears when `PLLE=0`.                  |
| `PLLFRQ` |             data `0x52` | `PINMUX`, `PLLUSB`, `PLLTM1:0`, `PDIV3:0` | Selects PLL input, USB postscaler, timer postscaler, and PLL output frequency. For 96 MHz PLL output, `PLLUSB=1` divides by 2 to provide 48 MHz USB clock. |

`PLLFRQ.PINMUX=1` connects the PLL input directly to the internal calibrated 8 MHz RC oscillator, which the datasheet ties to low-speed crystal-less operation. `PLLFRQ.PLLUSB=0` sends a 48 MHz PLL output directly to USB; `PLLUSB=1` divides a 96 MHz PLL output by 2 for USB. ([Microchip][1])

For a cycle emulator, model the USB clock as an independent 48 MHz domain when active. If `f_CPU=16 MHz`, that is exactly 3 USB ticks per CPU cycle. If `f_CPU` differs, schedule USB events in absolute time or rational clock ticks.

---

## 4. Physical attach, speed selection, VBUS, and pad behavior

The peripheral selects speed by enabling an internal pull-up on either D+ or D−:

| `UDCON.LSM` | Mode       | Pull-up |
| ----------: | ---------- | ------- |
|           0 | Full-speed | D+      |
|           1 | Low-speed  | D−      |

The pull-up is connected only when the device is logically attached: `UDCON.DETACH=0`, the USB macro is enabled, and VBUS is valid. Setting `DETACH` disconnects the internal pull-up; clearing it reconnects the pull-up. The datasheet explicitly notes that a device can be forced to re-enumerate by setting and clearing `DETACH`, with firmware accounting for debounce time. ([Microchip][1])

VBUS detection is handled through the VBUS pad. The datasheet says the `USBSTA.VBUS` status bit becomes set when the session-valid signal is active, with VBUS above about 1.4 V; `USBINT.VBUSTI` is set on VBUS transitions. The peripheral cannot attach while `VBUS` is not set. ([Microchip][1])

Pad suspend behavior is also modeled externally: when suspend is detected, `SUSPI` is set and the USB pad is put into idle/low-power mode; a non-idle bus event sets `WAKEUPI` and wakes the pad. `DETACH=1` also places the pad in idle behavior until cleared. ([Microchip][1])

---

## 5. Global USB operating states

A useful global state machine:

```text
Hardware reset
  -> USB disabled/reset, USBE=0, FRZCLK=1, pad suspended, internal state reset

USBE set
  -> Device idle state, USB macro enabled, still detached until DETACH=0

VBUS valid and DETACH cleared
  -> Pull-up active, host sees device attach

Host USB reset
  -> Endpoint state reset, endpoints disabled except default control endpoint,
     address returns to 0, EORSTI set

Configured / active transfers
  -> Endpoint transaction state machines handle SETUP/IN/OUT/SOF/NAK/STALL

Suspend after 3 ms inactivity
  -> SUSPI set, pad idle, firmware may set FRZCLK and stop PLL

Wake/resume
  -> WAKEUPI/EORSMI/UPRSMI events, firmware restarts PLL and clears FRZCLK

USBE cleared
  -> USB controller disabled and reset
```

After ordinary hardware resets — power-on, external, watchdog, brown-out, JTAG — the USB controller is disabled and reset. `USBE` is not set, `FRZCLK` is set, the USB clock is stopped, the pad is in suspend mode, and the internal USB state is reset. Setting `USBE` enters the device idle state; clearing `USBE` later acts like a hardware reset of the USB controller. ([Microchip][1])

The 32U4 also supports a special **USB reset CPU reset** mode. If `UDCON.RSTCPU` is set and the controller detects a USB End-of-Reset event, the CPU core is reset while the USB controller remains enabled and attached. ([Microchip][1])

---

## 6. USB reset behavior

A USB bus reset is detected as SE0 for at least 2.5 µs. On USB reset:

```text
- all endpoints are disabled
- default control endpoint remains configured
- device address returns to 0
- ADDEN is cleared
- EORSTI is set
- optional CPU reset occurs if RSTCPU=1
```

The datasheet states that on USB reset all endpoints are disabled, the default control endpoint remains configured, and if `RSTCPU` is enabled the CPU is reset while the USB controller remains attached. `UDADDR.ADDEN` is cleared after power-up reset, USB reset, or `USBE` being cleared. ([Microchip][1])

For emulation, treat bus reset as a protocol-level event independent of CPU reset unless `RSTCPU=1`. If `RSTCPU=1`, reset CPU-visible core state but preserve the USB controller’s enabled/attached condition as specified.

---

## 7. Address setup

After power-up or USB reset, the device responds to address 0. During `SET_ADDRESS`, firmware writes the new address to `UDADDR.UADD6:0` but leaves `ADDEN=0` until after it has completed the zero-length IN status stage. Then firmware sets `ADDEN`, after which only the programmed address is accepted. `ADDEN` and `UADD` should not be written at the same time. ([Microchip][1])

Emulator behavior:

```text
if ADDEN == 0:
    accepted_address = 0
else:
    accepted_address = UDADDR.UADD
```

For `SET_ADDRESS`, do not switch the accepted address immediately when firmware writes `UADD`; switch only when firmware later writes `ADDEN=1`.

---

## 8. Endpoint configuration and DPRAM allocation

Endpoint selection is via:

```text
UENUM.EPNUM2:0 = endpoint number 0..6
EPNUM = 7 is forbidden
```

Endpoint activation flow:

```text
1. Select endpoint in UENUM.
2. Set UECONX.EPEN.
3. Program UECFG0X: endpoint type and direction.
4. Program UECFG1X: endpoint size, bank count, ALLOC.
5. Hardware updates UESTA0X.CFGOK.
```

As long as `CFGOK=0`, the hardware does not acknowledge packets sent by the host. `CFGOK` is set when the endpoint size and bank parameters are valid for that endpoint’s FIFO capacity and bank count. ([Microchip][1])

### Endpoint type and direction

`UECFG0X`:

| Bits        | Meaning                                                                                      |
| ----------- | -------------------------------------------------------------------------------------------- |
| `EPTYPE1:0` | `00` control, `01` isochronous, `10` bulk, `11` interrupt                                    |
| `EPDIR`     | 1 = IN for bulk/interrupt/isochronous; 0 = OUT. Control endpoints use OUT direction setting. |

The datasheet defines these type encodings and states that `EPDIR=1` configures IN for bulk, interrupt, or isochronous endpoints; `EPDIR=0` configures OUT for bulk, interrupt, isochronous, or control. ([Microchip][1])

### Endpoint size and banks

`UECFG1X`:

| `EPSIZE2:0` |                                             Size |
| ----------: | -----------------------------------------------: |
|       `000` |                                          8 bytes |
|       `001` |                                         16 bytes |
|       `010` |                                         32 bytes |
|       `011` |                                         64 bytes |
|       `100` |                                        128 bytes |
|       `101` |                                        256 bytes |
|       `110` | 512 bytes, reserved/not valid for 32U4 endpoints |
|       `111` |                                         reserved |

`EPBK1:0` selects one bank or double bank: `00` = one bank, `01` = double bank, `1x` reserved. `ALLOC` reserves or frees endpoint DPRAM. ([Microchip][1])

### DPRAM allocation rule

This is important for emulation. Endpoint memory must be allocated in increasing endpoint order. When endpoint `k` is allocated, the hardware inserts its memory after endpoint `k-1`, and endpoint `k+1` may slide; higher endpoints do not necessarily slide. Clearing `EPEN` does **not** free memory. To free memory, firmware must clear `ALLOC`. The datasheet warns that reconfiguration can create memory conflicts and that `CFGOK` may still be set even in some conflict cases. ([Microchip][1])

For a high-fidelity emulator, implement DPRAM allocation as actual base/limit regions, not just per-endpoint abstract queues. This matters if firmware configures endpoints out of order or reconfigures them dynamically.

---

## 9. Endpoint reset and disable behavior

`UERST.EPRST6:0` resets endpoint FIFOs. The reset operation:

```text
- resets endpoint internal state machine
- clears RX and TX banks
- restores internal pointers
- restores UEINTX, UESTA0X, UESTA1X to reset values
- leaves data toggle unchanged
- leaves configuration active
- leaves endpoint enabled
```

Firmware sets the relevant `EPRSTx` bit and then clears it to complete reset and resume use. ([Microchip][1])

Clearing `UECONX.EPEN` acts as an endpoint reset, but also resets the data toggle field, keeps endpoint configuration, and keeps DPRAM memory reserved. ([Microchip][1])

`UECONX.RSTDT` is the explicit data-toggle reset command. It clears the endpoint’s data toggle sequence and is automatically cleared by hardware immediately. ([Microchip][1])

---

## 10. FIFO and bank model

Each endpoint should have:

```c
struct UsbEndpoint {
    bool enabled;
    EndpointType type;
    Direction dir;
    unsigned size_bytes;
    unsigned num_banks;      // 1 or 2
    bool allocated;
    bool cfgok;

    Bank banks[2];
    unsigned curr_bank;
    unsigned busy_bank_count;

    bool stallrq;
    DataToggle next_in_toggle;
    DataToggle expected_out_toggle;
    // flags: TXINI, RXOUTI, RXSTPI, STALLEDI, NAKINI, NAKOUTI, etc.
};
```

Each bank should have:

```c
struct Bank {
    uint8_t data[256];       // max useful bank size on 32U4
    unsigned count;
    unsigned fifo_ptr;       // CPU read/write pointer
    bool busy;               // OUT: host-filled; IN: CPU-filled, waiting to send
    DataToggle data_toggle;
};
```

`UESTA0X.NBUSYBK1:0` reports the number of busy banks. For IN endpoints, busy means filled by firmware and ready for IN transfer. For OUT endpoints, busy means filled by the host. `UESTA1X.CURRBK1:0` reports the current bank for non-control endpoints. ([Microchip][1])

`UEDATX` reads/writes the FIFO byte of the currently selected endpoint. `UEBCLX` and `UEBCHX` expose an 11-bit byte count. For IN endpoints, byte count increases on CPU writes to `UEDATX` and decreases as bytes are sent. For OUT endpoints, byte count increases as host data arrives and decreases as software reads. ([Microchip][1])

---

## 11. Control endpoint behavior

Control endpoints are special.

A `SETUP` request is **always ACKed**. When a valid SETUP packet arrives, `RXSTPI` is set and `RXOUTI` is not set. `FIFOCON` and `RWAL` are irrelevant for control endpoints and should read as 0 for control endpoint use. ([Microchip][1])

Control endpoint flags:

| Flag     | Meaning                                                                                           |
| -------- | ------------------------------------------------------------------------------------------------- |
| `RXSTPI` | New SETUP packet received; firmware clears it to acknowledge and clear the bank.                  |
| `RXOUTI` | OUT data received during data/status stage; firmware clears it to acknowledge and clear the bank. |
| `TXINI`  | IN bank ready; firmware writes data or ZLP, then clears `TXINI` to send.                          |

The datasheet states that for control endpoints `RXSTPI`, `RXOUTI`, and `TXINI` are the management bits, and each is cleared by firmware to acknowledge the relevant packet/bank event. ([Microchip][1])

Important control-transfer details:

1. **SETUP priority:** A new SETUP has priority over any other request and must be ACKed. On a new SETUP, other flags/FIFO state should be reset as described.
2. **Control read status stage:** The first OUT status-stage command is always NAKed. When the controller detects the status stage, data already written by the CPU is erased, and clearing `TXINI` has no effect.
3. **OUT ZLP warning:** The byte counter is reset when the OUT zero-length packet is received.
4. **STALL special case:** If `STALLRQ` is set on a control endpoint and a SETUP arrives, the SETUP is ACKed and `STALLRQ`, `STALLEDI`, `TXINI`, etc. are automatically reset. ([Microchip][1])

---

## 12. OUT endpoint behavior

For non-control OUT endpoints:

```text
Host sends OUT DATA
  if endpoint enabled, configured, matching address, bank available, no stall:
      accept packet
      fill current bank
      set RXOUTI and FIFOCON
      set/update RWAL
      increment NBUSYBK
      ACK host, except isochronous has no handshake at USB protocol level
  else:
      NAK or STALL depending state/type
```

The CPU-side flow:

```text
1. Poll or interrupt on RXOUTI/FIFOCON.
2. Read BYCT from UEBCHX:UEBCLX.
3. Read N bytes from UEDATX.
4. Clear RXOUTI.
5. Clear FIFOCON to free the bank and advance to next bank.
```

The datasheet explicitly states that `RXOUTI` must always be cleared before `FIFOCON`. In double-bank mode, while the CPU reads bank 0, the host may fill bank 1; when firmware clears `FIFOCON`, the next bank may already be ready and `RXOUTI` can be set immediately. ([Microchip][1])

`RWAL` for OUT means the current bank is not empty and firmware may read. It clears when the bank becomes empty. ([Microchip][1])

---

## 13. IN endpoint behavior

For non-control IN endpoints:

```text
When current bank is free:
    hardware sets TXINI and FIFOCON
    RWAL=1 while CPU may write

CPU writes data to UEDATX
    BYCT increments
    RWAL clears when bank full

CPU clears TXINI, then clears FIFOCON
    bank becomes ready for host IN token

Host sends IN token
    if bank ready:
        controller sends DATA packet
        after host ACK:
            bank becomes free
            BYCT decrements as bytes sent
            TXINI/FIFOCON may set for next free bank
    else:
        send NAK and set NAKINI
```

The datasheet states that `TXINI` is set when the current bank becomes free; `FIFOCON` is set at the same time. Firmware writes FIFO data and clears `FIFOCON` to allow the controller to send. `TXINI` must always be cleared before clearing `FIFOCON`. In double-bank mode, one bank can be sent by the host side while the CPU writes the other. ([Microchip][1])

`RWAL` for IN means the current bank is not full and firmware may write. It clears when the bank is full or otherwise not writable. ([Microchip][1])

`UEINTX.KILLBK` shares the bit position with `RXOUTI` for IN endpoints. Writing it kills the last written bank; hardware clears it when done. This is used for abort cases, such as a control transaction receiving an OUT ZLP during an IN stage. ([Microchip][1])

---

## 14. STALL, NAK, retry, and data toggle

`UECONX.STALLRQ` requests STALL handshakes. Once set, all following applicable requests are handshaked with STALL until firmware sets `STALLRQC`. Setting `STALLRQC` automatically clears `STALLRQ`, and `STALLRQC` itself is immediately cleared by hardware. Each sent STALL sets `STALLEDI`; incoming packets are discarded and do not set `RXOUTI` or `RWAL`. ([Microchip][1])

The retry mechanism has priority over STALL: a STALL is sent only if `STALLRQ` is set and no retry is required. ([Microchip][1])

Data toggle state is visible in `UESTA0X.DTSEQ1:0`. For OUT, it reports the data toggle received on the current bank. For IN, it reports the toggle that will be used for the next packet to be sent, not necessarily the current bank. `RSTDT` resets the sequence so the next packet uses DATA0. ([Microchip][1])

---

## 15. Isochronous and error behavior

Isochronous endpoints have special error reporting:

| Condition                    | Behavior                                                                                                 |
| ---------------------------- | -------------------------------------------------------------------------------------------------------- |
| IN underflow                 | Host attempts to read an empty bank; `UNDERFI` set.                                                      |
| OUT underflow/full condition | Host sends while banks are full; packet lost.                                                            |
| OUT CRC error                | `STALLEDI` set; `RXOUTI` can still be triggered.                                                         |
| OUT overflow                 | Host sends packet larger than bank; `OVERFI` set, packet acknowledged, `RXOUTI` set, first bytes stored. |

The datasheet states that overflow can occur for control, isochronous, bulk, or interrupt OUT if the host writes a packet too large for the bank; the bank contains the first bytes of the packet. It also states that IN-side overflow and OUT-side CPU underflow are not possible if firmware obeys `TXINI/RWAL` and `RXOUTI/RWAL`. ([Microchip][1])

For a high-fidelity emulator, do not simply reject oversized OUT packets. Accept the packet, set `OVERFI`, set `RXOUTI`, and store the truncated initial bytes as specified.

---

## 16. Suspend, wake-up, and remote wake-up

Suspend is detected after 3 ms of USB line inactivity. `UDINT.SUSPI` is set, and firmware may set `USBCON.FRZCLK` to freeze the USB clock. Recovery can happen either by firmware clearing `FRZCLK`, or by enabling `WAKEUPE` so a non-idle bus signal triggers `WAKEUPI`. `WAKEUPI` and `SUSPI` are not strictly paired; either can occur independently, and the datasheet says each clears the other if it was already set. ([Microchip][1])

Remote wake-up uses `UDCON.RMWKUP`. The device must first be suspended and have remote wake-up enabled by the host. Firmware sets `RMWKUP`; after 5 ms of inactivity, the controller sends upstream resume, sets `UPRSMI`, clears `SUSPI`, and eventually clears `RMWKUP`. A good End-of-Resume from the host sets `EORSMI`. ([Microchip][1])

When `FRZCLK=1`, only a restricted set of registers remains accessible, and only `WAKEUPI` and `VBUSTI` can be triggered. The datasheet lists `USBCON`, `USBSTA`, `USBINT`, `UDCON`, `UDINT`, and `UDIEN` as accessible while frozen. ([Microchip][1])

---

## 17. Interrupt model

The USB peripheral has two interrupt vectors:

```text
USB general interrupt vector:
    VBUS, suspend, wake-up, SOF, reset, resume events

USB endpoint interrupt vector:
    any enabled endpoint event on endpoint 0..6
```

General device interrupt sources:

| Flag            | Enable          | Meaning                    |
| --------------- | --------------- | -------------------------- |
| `USBINT.VBUSTI` | `USBCON.VBUSTE` | VBUS transition            |
| `UDINT.UPRSMI`  | `UDIEN.UPRSME`  | Upstream resume being sent |
| `UDINT.EORSMI`  | `UDIEN.EORSME`  | End of resume detected     |
| `UDINT.WAKEUPI` | `UDIEN.WAKEUPE` | Non-idle bus wake-up       |
| `UDINT.EORSTI`  | `UDIEN.EORSTE`  | End of USB reset           |
| `UDINT.SOFI`    | `UDIEN.SOFE`    | SOF detected every 1 ms    |
| `UDINT.SUSPI`   | `UDIEN.SUSPE`   | Suspend after inactivity   |

The datasheet classifies VBUS, upstream resume, end-of-resume, wake-up, end-of-reset, SOF, and suspend as processing interrupts, with SOF CRC error indicated via `FNCERR`. ([Microchip][1])

Endpoint interrupt sources:

| Flag              | Enable            | Meaning                           |
| ----------------- | ----------------- | --------------------------------- |
| `UEINTX.TXINI`    | `UEIENX.TXINE`    | IN bank ready / transmitter ready |
| `UEINTX.STALLEDI` | `UEIENX.STALLEDE` | STALL sent or iso OUT CRC error   |
| `UEINTX.RXOUTI`   | `UEIENX.RXOUTE`   | OUT data received                 |
| `UEINTX.RXSTPI`   | `UEIENX.RXSTPE`   | SETUP received                    |
| `UEINTX.NAKOUTI`  | `UEIENX.NAKOUTE`  | NAK sent to OUT/PING              |
| `UEINTX.NAKINI`   | `UEIENX.NAKINE`   | NAK sent to IN                    |
| `UESTA0X.OVERFI`  | `UEIENX.FLERRE`   | Overflow error                    |
| `UESTA0X.UNDERFI` | `UEIENX.FLERRE`   | Underflow error                   |

`UEINT` is a read-only summary of which endpoints currently have enabled pending interrupt sources. The datasheet says `UEINT.EPINT6:0` is set by hardware when an enabled `UEINTX` source triggers for the corresponding endpoint and cleared when the source is served. ([Microchip][1])

Important flag-clearing convention: most USB interrupt flags are hardware-set and software-cleared; setting them by software has no effect. In practice, AVR USB firmware usually clears these flags by writing the bit to 0 while preserving other set flags.

---

## 18. Register reference

Addresses below are data-space addresses unless noted. `PLLCSR` is in the low I/O range, where AVR also has a separate I/O address.

### USB general / hardware registers

| Register | Address | Bits                                  | Behavior                                                                                   |
| -------- | ------: | ------------------------------------- | ------------------------------------------------------------------------------------------ |
| `UHWCON` |  `0xD7` | `UVREGE`                              | Enables USB pad regulator.                                                                 |
| `USBCON` |  `0xD8` | `USBE`, `FRZCLK`, `OTGPADE`, `VBUSTE` | Enables USB macro, freezes USB clock, enables VBUS pad, enables VBUS transition interrupt. |
| `USBSTA` |  `0xD9` | `ID`, `VBUS`                          | `ID` always reads 1 for AT90USB compatibility; `VBUS` reflects VBUS/session-valid state.   |
| `USBINT` |  `0xDA` | `VBUSTI`                              | Set on VBUS transition; interrupt if `VBUSTE=1`.                                           |

The datasheet states that `USBE=0` disables and resets the controller, transceiver, and clock inputs; `FRZCLK=1` disables USB clock inputs while resume detection remains active; `OTGPADE` enables the VBUS pad even if `USBE=0`; `VBUSTE` enables VBUS transition interrupt generation. ([Microchip][1])

### USB device global registers

| Register  | Address | Bits                                                     | Behavior                                                                      |
| --------- | ------: | -------------------------------------------------------- | ----------------------------------------------------------------------------- |
| `UDCON`   |  `0xE0` | `RSTCPU`, `LSM`, `RMWKUP`, `DETACH`                      | CPU reset on USB reset, low-speed selection, remote wake-up, physical detach. |
| `UDINT`   |  `0xE1` | `UPRSMI`, `EORSMI`, `WAKEUPI`, `EORSTI`, `SOFI`, `SUSPI` | Device interrupt flags.                                                       |
| `UDIEN`   |  `0xE2` | `UPRSME`, `EORSME`, `WAKEUPE`, `EORSTE`, `SOFE`, `SUSPE` | Device interrupt enables.                                                     |
| `UDADDR`  |  `0xE3` | `ADDEN`, `UADD6:0`                                       | Device address and address-enable sequencing.                                 |
| `UDFNUML` |  `0xE4` | `FNUM7:0`                                                | Lower frame number bits.                                                      |
| `UDFNUMH` |  `0xE5` | `FNUM10:8`                                               | Upper frame number bits.                                                      |
| `UDMFN`   |  `0xE6` | `FNCERR`                                                 | SOF frame-number CRC error flag.                                              |

`UDFNUMH:UDFNUML` contains the 11-bit frame number from the last received SOF, and the datasheet notes that frame number is updated even if a corrupted SOF is received. `UDMFN.FNCERR` is set when a corrupted SOF frame number is received and is updated with `SOFI`. ([Microchip][1])

### Endpoint selection and endpoint window registers

| Register  | Address | Bits                                                                                   | Behavior                                           |
| --------- | ------: | -------------------------------------------------------------------------------------- | -------------------------------------------------- |
| `UEINTX`  |  `0xE8` | `FIFOCON`, `NAKINI`, `RWAL`, `NAKOUTI`, `RXSTPI`, `RXOUTI/KILLBK`, `STALLEDI`, `TXINI` | Main selected-endpoint flags/control.              |
| `UENUM`   |  `0xE9` | `EPNUM2:0`                                                                             | Selects endpoint 0–6; 7 forbidden.                 |
| `UERST`   |  `0xEA` | `EPRST6:0`                                                                             | Endpoint FIFO reset bits.                          |
| `UECONX`  |  `0xEB` | `STALLRQ`, `STALLRQC`, `RSTDT`, `EPEN`                                                 | Stall control, reset data toggle, endpoint enable. |
| `UECFG0X` |  `0xEC` | `EPTYPE1:0`, `EPDIR`                                                                   | Endpoint type and direction.                       |
| `UECFG1X` |  `0xED` | `EPSIZE2:0`, `EPBK1:0`, `ALLOC`                                                        | Endpoint size, bank count, DPRAM allocation.       |
| `UESTA0X` |  `0xEE` | `CFGOK`, `OVERFI`, `UNDERFI`, `DTSEQ1:0`, `NBUSYBK1:0`                                 | Config/status/error/toggle/busy-bank flags.        |
| `UESTA1X` |  `0xEF` | `CTRLDIR`, `CURRBK1:0`                                                                 | Control direction and current bank.                |
| `UEIENX`  |  `0xF0` | `FLERRE`, `NAKINE`, `NAKOUTE`, `RXSTPE`, `RXOUTE`, `STALLEDE`, `TXINE`                 | Selected endpoint interrupt enables.               |
| `UEDATX`  |  `0xF1` | `DAT7:0`                                                                               | FIFO byte read/write.                              |
| `UEBCLX`  |  `0xF2` | `BYCT7:0`                                                                              | FIFO byte count low.                               |
| `UEBCHX`  |  `0xF3` | `BYCT10:8`                                                                             | FIFO byte count high.                              |
| `UEINT`   |  `0xF4` | `EPINT6:0`                                                                             | Endpoint interrupt summary.                        |

The official register summary places the endpoint window from `0xE8` through `0xF4`, with global USB device registers at `0xE0`–`0xE6` and general USB hardware registers at `0xD7`–`0xDA`. ([Microchip][1])

---

## 19. Cycle-accurate emulation guidance

### Timing domains

Use at least these clocks:

```text
CPU clock:
    executes AVR instructions and MMIO reads/writes

USB peripheral clock:
    48 MHz when USBE=1, FRZCLK=0, PLL locked, regulator/pad enabled

USB bus bit clock:
    full-speed: 12 Mbit/s = 4 USB-clock ticks per bit at 48 MHz
    low-speed:  1.5 Mbit/s = 32 USB-clock ticks per bit at 48 MHz

USB frame clock:
    SOF every 1 ms in full-speed active operation
```

The datasheet directly states that the 48 MHz clock is used to generate 12 MHz full-speed or 1.5 MHz low-speed bit timing. ([Microchip][1])

### Event timing assumptions

The datasheet does **not** specify exact CPU-cycle timing for every internal flag transition. A practical high-fidelity model should define stable rules such as:

| Event            | Recommended emulator timing                                                       |
| ---------------- | --------------------------------------------------------------------------------- |
| `VBUSTI`         | On modeled VBUS comparator transition.                                            |
| `EORSTI`         | At end of SE0 reset condition, after reset effects applied.                       |
| `SOFI`           | At successful SOF token/frame decode; update frame number first, then set `SOFI`. |
| `SUSPI`          | After 3 ms line inactivity.                                                       |
| `WAKEUPI`        | On filtered non-idle line state while wake detection active.                      |
| `RXSTPI`         | After valid SETUP packet is accepted into endpoint 0 FIFO.                        |
| `RXOUTI`         | After valid OUT packet is accepted into selected OUT endpoint bank.               |
| `TXINI`          | When an IN bank becomes free; after host ACK for a sent IN packet.                |
| `NAKINI/NAKOUTI` | When the controller emits a NAK handshake.                                        |
| `STALLEDI`       | When the controller emits a STALL handshake.                                      |
| `OVERFI/UNDERFI` | At the point the corresponding transfer error is detected.                        |

This will match software-visible behavior for most firmware. Exact cycle differences between “end of packet,” “ACK handshake sent/received,” and “flag visible to CPU” are not fully specified by the datasheet, so those are candidates for silicon tests if you need regression-perfect behavior.

### CPU read/write race behavior

For cycle accuracy, handle races explicitly:

```text
At each CPU cycle:
    1. Execute CPU memory access, if any.
    2. Apply CPU register write effects.
    3. Advance USB clock ticks belonging to this CPU cycle.
    4. Generate USB events and interrupt pending state.
```

Or reverse steps 1 and 3 if your emulator defines peripherals-before-CPU for the AVR core; the important part is to make the ordering deterministic. Firmware that polls `UEINTX`, `UDINT`, or `UEINT` can observe one-cycle differences depending on this ordering.

### Endpoint FIFO edge cases to implement

Do not skip these if you want robust firmware compatibility:

```text
- UENUM selects the endpoint register bank.
- EPNUM=7 is forbidden; reads can return zeros or leave behavior undefined.
- Clearing EPEN resets endpoint state but keeps config and allocation.
- UERST resets FIFO/pointers/status but not data toggle.
- RSTDT resets data toggle immediately and self-clears.
- ALLOC changes DPRAM allocation and can slide/conflict endpoint memory.
- CFGOK checks size/bank legality, not necessarily every memory conflict.
- RXOUTI must be cleared before FIFOCON.
- TXINI must be cleared before FIFOCON.
- Control endpoint ignores RWAL/FIFOCON semantics.
- SETUP always ACKs and preempts pending control transfer state.
- STALLRQ on control endpoint is cleared by a new SETUP.
- KILLBK is the write meaning of RXOUTI bit on IN endpoints.
- BYCT changes on every UEDATX CPU read/write and on USB send/receive.
- Double-bank endpoints can immediately assert TXINI/RXOUTI after bank switch.
```

Most of these behaviors come directly from the endpoint activation, endpoint reset, control endpoint, OUT endpoint, IN endpoint, and register descriptions. ([Microchip][1])

---

## 20. Minimal implementation checklist

A good emulator implementation target would include:

```text
Global:
  UHWCON, USBCON, USBSTA, USBINT
  PLLCSR, PLLFRQ, optional CLKSEL/CLKSTA/CLKPR interaction
  UDCON, UDINT, UDIEN, UDADDR, UDFNUMH/L, UDMFN
  MCUSR.USBRF if modeling USB reset CPU-reset source

Bus:
  VBUS validity
  D+/D- pull-up from LSM + DETACH + VBUS + USBE
  SE0 reset detection >= 2.5 us
  suspend detection after 3 ms inactivity
  resume and remote wake-up timing

Endpoint array:
  7 endpoint objects
  per-endpoint type, direction, size, banks, allocation
  per-bank data, byte count, current pointer, busy/free state
  data toggle state
  stall state
  selected-endpoint register window

Protocol:
  address filter using ADDEN/UADD
  SETUP, IN, OUT, SOF, reset, suspend, resume
  ACK/NAK/STALL behavior
  bulk/interrupt/control retry semantics
  isochronous no-handshake/error behavior

Interrupts:
  general vector pending from USBINT/UDINT and enables
  endpoint vector pending from UEINTX/UEIENX per endpoint
  UEINT summary bits
```

The biggest “gotchas” are **selected endpoint windowing**, **DPRAM allocation/sliding**, **control endpoint SETUP priority**, **flag clear semantics**, and **double-bank immediate reassertion**. Those are the behaviors most likely to break real 32U4 USB stacks if modeled only as simple queues.

[1]: https://ww1.microchip.com/downloads/en/devicedoc/atmel-7766-8-bit-avr-atmega16u4-32u4_datasheet.pdf "ATmega16U4/32U4 Datasheet"
