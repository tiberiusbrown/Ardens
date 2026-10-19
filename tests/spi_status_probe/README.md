# SPI status acknowledgment priority probe

This ATmega32U4 / 16 MHz Arduboy program measures the hardware cases left
unspecified by section 17.2.2 of the
[datasheet](https://ww1.microchip.com/downloads/en/DeviceDoc/Atmel-7766-8-bit-AVR-ATmega16U4-32U4_Datasheet.pdf):
an SPSR read, an intervening transfer completion, and an SPDR access.
It records all results with interrupts disabled before initializing the OLED.
SPI is master mode 0 at F_CPU/2; OLED and FX/FX-C chip selects stay high during
the measurements. The application detaches USB; use the usual bootloader/reset
procedure to reflash afterward.

## Run and report

Flash `spi_status_probe.hex` to the Arduboy, then record all three OLED pages.
A advances the page; B goes back. The header is `53 50 01 PP 00 00`, with page
`PP` equal to `00`, `01`, or `02`. A photograph of each page or a transcription
of the six data rows per page is sufficient.

Every row contains six hexadecimal bytes:

```text
ID  BEFORE  AFTER  LATE  ACCESS_RX  LATE_RX
```

`BEFORE` is the deliberate SPSR read that can arm acknowledgment. `AFTER` is
SPSR one CPU cycle after the measured SPDR access. `LATE` is SPSR 34 cycles
after that access, allowing any newly launched transfer to finish. The last
two fields record the received data at the measured access and afterward;
`ACCESS_RX=FF` is a placeholder for an OUT case. MISO is pulled up and all
external SPI devices are deselected; received data is ancillary to the flag
measurements.

| IDs | Flags present at BEFORE | Measured SPDR access | Access gaps |
| --- | --- | --- | --- |
| 00, 01, 02 | Old SPIF, current transfer busy | IN | 16, 17, 18 |
| 03, 04, 05 | Old SPIF, current transfer busy | OUT | 16, 17, 18 |
| 06, 07, 08 | WCOL only, current transfer busy | IN | 16, 17, 18 |
| 09, 0A, 0B | WCOL only, current transfer busy | OUT | 16, 17, 18 |
| 0C, 0D, 0E | Neither flag, current transfer busy | IN | 16, 17, 18 |
| 0F, 10, 11 | Neither flag, current transfer busy | OUT | 16, 17, 18 |

Gaps count CPU cycles from the OUT that starts the current transfer to the
measured SPDR access. In the old-SPIF cases an earlier transfer starts at
cycle 0, the current transfer at 18, and BEFORE is sampled at 19. In the
WCOL-only cases the transfer starts at 0, a collision is generated at 6, and
BEFORE is sampled at 7. The empty-flag controls sample BEFORE at 7 without a
collision. The timed instructions are in single inline-assembly blocks.

SPI2X contributes bit 0 to every status sample: `01` means neither flag,
`81` means SPIF, `41` means WCOL, and `C1` means both. Interpret each case with
its timestamps: AFTER for a cycle-16 access is sampled at cycle 17, whereas
AFTER for a cycle-17 access is sampled at cycle 18.

The physical FX-C results are transcribed in `hardware-results.txt` from the
three supplied photographs. They show that acknowledgment survives completion
and clears only flags that the earlier SPSR read observed set. They also show
SPIF absent at cycle 17 and present at 18, and WCOL after a cycle-17 OUT.
The follow-up in [RECEIVE_PROBE.md](RECEIVE_PROBE.md) confirms that a cycle-17
dummy OUT advances the flash stream despite setting WCOL. Its two physical
pages are transcribed in [receive-hardware-results.txt](receive-hardware-results.txt).
Simulator previews are labeled MODEL ONLY.

## Measured behavior and emulator changes

Times count CPU cycles from the initiating OUT, using the instruction-handler
cycle convention in `tests/spi_tests.cpp`. These captures measure master mode 0
at F_CPU/2, with interrupts disabled.

| Access cycle | Received data | New SPDR write | Fresh SPIF |
| --- | --- | --- | --- |
| 16 | Available | Ignored, WCOL set | Absent |
| 17 | Available | Dummy transfer starts, WCOL set | Absent |
| 18 | Available | Requested byte accepted normally | Set |

An unacknowledged old flag may already be set at any of these times. An SPSR
read arms acknowledgment independently for each flag observed set; the next
SPDR read **or write** clears those flags, even if another transfer completed
between the two accesses. An accepted write without an armed acknowledgment
preserves old flags. An SPSR write changes only SPI2X.

The core now delivers and latches the receive byte at 16, retains the writable
boundary at 17, and schedules SPIF at 18. Starting another transfer at 17 keeps
the preceding SPIF event. The existing zero-transmit rule for a nonzero OUT at
17 is retained from the original Ardens implementation; these new captures
directly establish the dummy-zero case and WCOL, rather than measuring a
nonzero MOSI byte. Slower dividers retain their previous timing. Other SPI
modes have regression controls but were not measured on hardware here.

Snapshots and savestates now serialize the delayed SPIF deadline and the
independent acknowledgment state. The format version is **0.25.12**; older
formats are rejected as incompatible instead of loading incomplete state.

## Regression validation

The integration target runs the 18 photographed status rows verbatim and the
nine timed receive/streaming rows with distinct fixture flash bytes. The latter
normalize hardware ID `EF 40 18` to fixture data `3C A7 52`, keeping CPU timing
independent of the flash model's existing JEDEC-ID response offset. Deselected
MISO also remains zero in the emulator, rather than the pulled-up hardware FF.
Neither discrepancy is hidden by an SPI adapter.

The cases pass under ordinary, merged, single-step, continuous, and continuous
unmerged execution. Additional checks cover nonzero/dummy writes at 16/17/18,
peripheral delivery, stale-flag polling, SPSR read-only bits, clock/mode/order
controls, and snapshot/reset replay through all three completion phases,
including a pending old SPIF after a new OUT at 17.

Both the complete GCC 13.3 Release suite using the GitHub CI configuration and
the MSVC RelWithDebInfo suite pass, including all existing image references.
Run the focused cases with `integration_tests --spi`, or omit `--spi` for the
full integration suite. No AVR toolchain is needed for these automated tests.

## AVM follow-up validation

The AVM RPG device harness was linked directly against the repaired Ardens
core. Its SPI observer logs writes without changing flags, timing, TX, or RX;
the exploratory status-fixing adapter is absent. The old diagnostic still
rejects the premature address write and displays, decoded from OLED pixels:

```text
06 00 03 00 B3 00 00 00 50 81
```

This reproduces the correct timed `B3` versus an incorrect polled byte; it does
not reproduce hardware's deselected `FF` byte. The corrected FX, FX-C, and
diagnostic FX-C packages pass new-game exploration, peaceful music, dialogue,
menus, scrolling, save/restart/load, and corrupt-save rejection. Tested SHA-256:

| Package | SHA-256 |
| --- | --- |
| Old diagnostic FX-C | `b4d61dcc6b1647b6c624ed5664fbb6c43c5c40e7e208173de226fbd0d3b25df4` |
| Corrected FX-C | `1ac7f5d494e1543781484e3d23d5fca60a5732d624f46f43626e0707b6f3da07` |
| Corrected FX | `13c129d07d67836e29671587ba4298dd9f0cd9298055117eef8e309bc5f3971d` |
| Corrected diagnostic FX-C | `95e271b4d1d3bffa258fef006c66fe438ebaf84bd82e61298b9be177d3c1bf4a` |

Corrected firmware makes intentional dummy OUTs at 17 and therefore sets WCOL.
Those runs disable the debugger's WCOL stop while leaving native WCOL set and
logging collisions; no ignored busy writes occur. With the WCOL stop enabled,
the old diagnostic stops at its ignored address write (byte PC `0x3760`).
The original physical AVM black-screen problem remains unresolved; emulator
validation does not establish that the corrected packages fix it on hardware.

## Build

From the repository root, with AVR GCC on PATH:

```powershell
New-Item -ItemType Directory -Path build-spi/status-probe -Force
avr-g++ -mmcu=atmega32u4 -DF_CPU=16000000UL -Os -std=gnu++11 -ffunction-sections -fdata-sections '-Wl,--gc-sections' tests/spi_status_probe/spi_status_probe.cpp -o build-spi/status-probe/spi_status_probe.elf
avr-objcopy -O ihex -R .eeprom build-spi/status-probe/spi_status_probe.elf build-spi/status-probe/spi_status_probe.hex
avr-objdump -d -S build-spi/status-probe/spi_status_probe.elf > build-spi/status-probe/spi_status_probe.lst
```

The current build uses 5,132 flash bytes and 91 SRAM bytes. The compiled probe
has been run in ordinary and merged emulator execution and its OLED pages
checked for legibility. The version-01 HEX and its hardware results are retained
unchanged; the version-02 receive probe is a separate binary.
