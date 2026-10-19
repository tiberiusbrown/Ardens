# Read-only FX-C byte-boundary probe (version 02)

The version-01 hardware captures show SPIF absent at cycle 17 and present at
18, and WCOL set by writes at both 16 and 17. They also establish independent
SPIF/WCOL acknowledgment across completion; the raw transcription is in
`hardware-results.txt`.

The status samples alone cannot distinguish a cycle-17 write that is ignored
from a zero-byte transfer that starts while also setting WCOL. This second
probe reads the flash's JEDEC ID to distinguish those cases with successive
received bytes. It also checks receive-buffer availability at 16/17/18 cycles.
It sends only release-from-power-down (`AB`), read-ID (`9F`), and dummy zero
bytes with the FX-C E2 chip select; it does not issue programming/erase commands.

Flash `build-spi/status-probe/spi_receive_probe.hex`. Report both pages
**00 and 01**; A or B switches pages. The header is **53 50 02 PP 00 00**.

Rows contain:

```text
ID  FIRST_RX  SECOND_RX  THIRD_RX  STATUS  LATE_STATUS
```

| ID | Measurement |
| --- | --- |
| 00 | Fully polled baseline: three successive ID bytes. |
| 01 | Two dummy OUTs 16 cycles apart. |
| 02 | Two dummy OUTs 17 cycles apart. |
| 03 | Two dummy OUTs 18 cycles apart. |
| 04 | Two dummy OUTs 19 cycles apart. |
| 05 | One dummy OUT; first SPDR read at 16 cycles. |
| 06 | One dummy OUT; first SPDR read at 17 cycles. |
| 07 | One dummy OUT; first SPDR read at 18 cycles. |
| 08 | Four dummy OUTs 17 cycles apart; collect the preceding RX at each handoff. |
| 09 | Four dummy OUTs 18 cycles apart; collect the preceding RX at each handoff. |
| 0A, 0B | Unused; all payload columns are FF. |

For rows 01-04, FIRST_RX is read one cycle after the second OUT, SECOND_RX
after 40 intervening NOPs, and THIRD_RX by a subsequent fully polled transfer.
Comparing with row 00 reveals whether the second OUT generated another flash
byte. For rows 05-07 the second read happens after a further 40 NOPs and the
third value comes from a fully polled transfer. Rows 08/09 directly compare
the repeated OUT/IN streaming patterns. Each case starts a fresh ID command.

The user's two physical FX-C pages are transcribed in
[receive-hardware-results.txt](receive-hardware-results.txt). Row 01 repeats
`EF` because the cycle-16 OUT is ignored. Row 02 advances through `EF 40 18`
despite WCOL, and row 08 confirms repeated cycle-17 handoffs. Row 05 already
receives `EF` at cycle 16 while SPSR at 17 is still `01`; RX availability and
SPIF visibility are separate phases. See the main [README](README.md) for the
resulting core behavior and validation.

Build from the repository root:

```powershell
avr-g++ -mmcu=atmega32u4 -DF_CPU=16000000UL -Os -std=gnu++11 -ffunction-sections -fdata-sections '-Wl,--gc-sections' tests/spi_status_probe/spi_receive_probe.cpp -o build-spi/status-probe/spi_receive_probe.elf
avr-objcopy -O ihex -R .eeprom build-spi/status-probe/spi_receive_probe.elf build-spi/status-probe/spi_receive_probe.hex
```

Built HEX SHA-256:
`3a6bf77b4a912df911b31890c4ae2d5db44dfe4ba2f974aff1607415f09afc94`.
The version-01 HEX is preserved separately with its original hash.
