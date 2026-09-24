# CYD SD Card Tester

A standalone SD card diagnostics, benchmark, recovery and low-level inspection tool for the **ESP32-2432S028 / Cheap Yellow Display (CYD)**.

The project is built for cards that are healthy, suspicious, partially readable, badly formatted, or simply refuse to behave.

Current development branch: **v0.8.7**

---

## What it does

The firmware can:

- detect SD cards at raw card level
- distinguish card initialization from filesystem mounting
- identify FAT12, FAT16, FAT32 and exFAT filesystems
- read card capacity
- read CID and CSD information
- show basic vendor hints from the manufacturer ID
- benchmark file write/read performance
- benchmark raw sector reads
- verify benchmark data with CRC
- test several SPI clock speeds
- display benchmark graphs on the CYD screen
- diagnose initialization failures
- run low-level raw SPI experiments through **SD Necromancer**
- fall back to software/bit-banged SPI for forensic testing

The project is deliberately evolving into a small SD-card toolbox rather than only a speed tester.

---

# Hardware

Target board:

**ESP32-2432S028 / Cheap Yellow Display**

The tested board uses a classic ESP32/WROOM-type module rather than ESP32-S3.

Display:

- 2.8" ILI9341
- 320 × 240
- resistive XPT2046 touchscreen

---

# Pinout

## TFT / ILI9341

| Signal | GPIO |
|---|---:|
| MISO | 12 |
| MOSI | 13 |
| SCLK | 14 |
| CS | 15 |
| DC | 2 |
| RESET | -1 |
| Backlight | 21 |

Display rotation:

```cpp
rotation = 1;
```

Resolution:

```text
320 × 240
```

---

## SD card

The SD card uses VSPI.

| Signal | GPIO |
|---|---:|
| CS | 5 |
| SCK | 18 |
| MISO | 19 |
| MOSI | 23 |

The firmware uses:

```cpp
SPIClass sdSPI(VSPI);
```

---

## XPT2046 Touch

Touch uses software SPI.

| Signal | GPIO |
|---|---:|
| CS | 33 |
| SCK | 25 |
| MOSI | 32 |
| MISO | 39 |
| IRQ | 36 |

Current calibration:

```cpp
TS_X_MIN = 250;
TS_X_MAX = 3850;

TS_Y_MIN = 250;
TS_Y_MAX = 3850;
```

Current coordinate mapping:

```cpp
screenX = map(rawY, TS_Y_MIN, TS_Y_MAX, 0, SCREEN_W - 1);
screenY = map(rawX, TS_X_MIN, TS_X_MAX, 0, SCREEN_H - 1);
```

---

# PlatformIO

Typical environment:

```ini
[env:cyd]
platform = espressif32
board = esp32dev
framework = arduino
```

The project uses:

- Arduino framework
- TFT_eSPI
- SdFat

For SD transfer performance, one important build option is:

```ini
-D USE_SPI_ARRAY_TRANSFER=1
```

---

# TFT_eSPI configuration

Important settings currently used by the project include:

```text
USER_SETUP_LOADED
ILI9341_2_DRIVER
USE_HSPI_PORT
LOAD_GLCD=1
LOAD_FONT2=1
LOAD_FONT4=1
SPI_FREQUENCY=40000000
```

The TFT and SD card use separate SPI peripherals/configurations.

---

# Main screen

The current interface exposes:

- **SCAN**
- **BENCH**
- **DIAG**
- **TOOLS**

The project intentionally keeps normal card testing separate from experimental/destructive recovery tools.

---

# Card scanning

The firmware distinguishes:

1. raw card initialization
2. volume/filesystem initialization

This is important because a card can be electrically alive and respond to SD commands while still having:

- a damaged partition table
- an unreadable filesystem
- no filesystem
- an unsupported filesystem
- corrupted boot sectors

The scanner therefore does not treat a filesystem mount failure as proof that the card itself is dead.

---

# Card qualification

A card is only considered properly readable when the normal scan can successfully perform more than `cardBegin()`.

Qualification includes:

- card initialization
- CID read
- CSD read
- raw sector 0 read

This avoids reporting a card as healthy merely because it responded during initialization.

---

# SPI speeds

Normal qualification currently tests:

```text
20 MHz
10 MHz
4 MHz
1 MHz
```

40 MHz is intentionally excluded from normal qualification because it has shown instability on the tested hardware/card setup.

The SPI scaling benchmark can still test:

```text
40 MHz
20 MHz
10 MHz
4 MHz
1 MHz
```

This makes 40 MHz useful as a diagnostic/performance data point without treating it as a reliable operating speed.

---

# Benchmark

The normal benchmark uses a temporary file:

```text
/SDTEST.BIN
```

Default test size:

```text
16 MiB
```

The benchmark performs:

1. file pre-allocation
2. write test
3. file read test
4. CRC verification
5. raw read benchmark
6. cleanup

The temporary benchmark file is removed afterwards.

---

# Benchmark performance

Earlier versions were limited to roughly:

```text
0.66 – 0.84 MB/s
```

The major performance improvement came from combining:

- `DEDICATED_SPI`
- `USE_SPI_ARRAY_TRANSFER=1`
- DMA-capable internal RAM buffers
- 32 KiB benchmark buffers
- removing `yield()` from timed transfer loops
- avoiding TFT updates during raw timed sections

After this work, measured performance reached approximately:

```text
Write:     ~1.9 – 2.0 MB/s
File read: ~2.15 MB/s
Raw read:  ~2.15 MB/s
```

Typical SPI efficiency measurements were approximately:

```text
20 MHz : 2.15 MB/s  ~86%
10 MHz : 1.15 MB/s  ~92%
4 MHz  : 0.48 MB/s  ~96%
1 MHz  : 0.12 MB/s  ~98%
```

Actual values depend on the card.

---

# Benchmark display

The Benchmark menu includes:

- quick benchmark
- write/read results
- raw read result
- CRC result
- bar chart
- SPI scaling test
- SPI scaling graph

The benchmark code is separated from the UI as much as practical so timing-sensitive SD operations do not depend directly on display rendering.

---

# DMA

Benchmark buffers are allocated from internal DMA-capable memory where possible.

The goal is to keep SD transfers on a fast path suitable for the ESP32 SPI driver.

The firmware can report whether the benchmark buffer is:

```text
DMA      YES/NO
INTERNAL YES/NO
```

---

# Diagnostics

The diagnostics screen is intended to expose useful failure information rather than only showing a generic "SD failed" message.

Current and planned diagnostic information includes:

- initialization state
- SdFat error code/data
- SPI clock used
- CID/CSD state
- raw read state
- card type
- filesystem state
- capacity

Future versions may decode more SdFat error values into human-readable command/stage names.

---

# Vendor hints

CID manufacturer IDs can be mapped to a vendor hint.

Examples used by the project include:

| MID | Hint |
|---:|---|
| 0x01 | Panasonic |
| 0x02 | Toshiba / Kioxia |
| 0x03 | SanDisk |
| 0x08 | Silicon Power |
| 0x11 | Sony |
| 0x1B | Samsung |
| 0x1D | ADATA |
| 0x27 | Phison |
| 0x28 | Lexar |
| 0x31 | Silicon Motion |
| 0x41 | Kingston |
| 0x74 | Transcend |
| 0x82 | Sony |

These are **hints only**.

A manufacturer ID is not sufficient proof that a card is authentic.

---

# Fake-capacity detection

The project does **not** label a card as authentic or fake based only on CID data.

A proper fake-capacity test requires writing and reading the claimed address space and verifying that unique data survives at the expected physical addresses.

A full destructive capacity verifier is planned.

---

# SD Necromancer

**SD Necromancer** is the experimental low-level recovery/forensics mode.

Location:

```text
TOOLS → SD NECROMANCER
```

This mode bypasses normal high-level SdFat initialization and talks to the card directly.

It exists for cards that:

- repeatedly fail normal initialization
- work only intermittently
- respond to some SD commands but not others
- behave differently depending on SPI speed
- appear dead to normal card readers
- remain stuck in SD idle state

---

# Necromancer safety

Necromancer is intentionally dangerous.

The UI requires explicit arming before destructive operations.

If a card becomes sufficiently responsive, the tool can:

1. read sector 0
2. save it in RAM
3. overwrite sector 0 with a known pattern
4. read the pattern back
5. verify it
6. restore the original sector
7. verify the restore

A failed restore can corrupt:

- MBR
- GPT metadata
- partition table
- filesystem boot sector

Do not use Necromancer on a card containing important data.

---

# Necromancer raw SPI

Current low-level tests include:

- extra startup clocks
- repeated CMD0
- CMD8
- CMD58
- CMD55
- ACMD41
- CMD1 fallback
- CMD13
- CMD16
- CMD59
- CID/CSD reads
- raw sector reads
- raw sector writes

SPI mode 0 is tested first.

An intentionally non-standard SPI mode 3 pass is also available as a forensic experiment.

---

# Necromancer SPI speeds

The raw hardware-SPI probe currently includes approximately:

```text
400 kHz
250 kHz
100 kHz
1 MHz
```

The software SPI cross-check runs considerably slower.

---

# Software / bit-banged SPI

A software-SPI implementation was added after an interesting debugging case where the ESP32 hardware SPI peripheral initially returned only `0x00`, while direct GPIO reads showed MISO high.

The software SPI probe:

- detaches VSPI
- drives SCK through GPIO
- drives MOSI through GPIO
- controls CS through GPIO
- samples MISO directly with `digitalRead()`
- reconstructs every received byte in software

Typical software-SPI target classes:

```text
~100 kHz
~50 kHz
~25 kHz
```

This provides an independent cross-check of the hardware SPI peripheral.

---

# Raw response tracing

Necromancer can print raw command responses such as:

```text
CMD0:
FF 01 FF FF FF ...

CMD8:
FF 01 00 00 01 AA ...

CMD58:
FF 01 40 FF 80 00 ...
```

This is useful because it exposes much more information than a single error code.

---

# SD R1 responses

Common R1 bits relevant during development include:

| Bit | Meaning |
|---:|---|
| 0 | Idle state |
| 1 | Erase reset |
| 2 | Illegal command |
| 3 | Command CRC error |
| 4 | Erase sequence error |
| 5 | Address error |
| 6 | Parameter error |

For example:

```text
0x01
```

means:

```text
Idle state
```

while:

```text
0x09
```

means:

```text
Idle state + CRC error
```

---

# CRC7

As of v0.8.5+, raw SD commands use a real SD command CRC7 implementation.

Sanity checks include:

```text
CMD0 CRC  = 0x95
CMD8 CRC  = 0x87
```

This matters when CMD59 enables CRC checking.

Earlier experimental builds exposed how easy it is to accidentally poison every later command by enabling CRC but continuing to send dummy CRC bytes.

---

# Payload-safe command parser

As of v0.8.7, command parsing separates:

- forensic response tracing
- normal R1 parsing

This matters because commands such as CMD8 and CMD58 return data immediately after R1.

A generic 32-byte trace must not be used internally by a normal parser, otherwise it consumes the payload before the caller can read it.

The normal parser now stops immediately after the R1 byte.

This is required for correct handling of:

- CMD8 R7
- CMD58 OCR / R3
- CID
- CSD
- data tokens
- sector reads

---

# Current suspicious-card test case

A Samsung EVO+ 32 GB card has been useful as a development stress case.

Observed behavior has included:

```text
CMD0  -> 0x01
CMD8  -> valid R7 response
CMD58 -> OCR response
CMD55 -> 0x01
ACMD41 -> remains 0x01
```

The controller therefore appears alive enough to:

- enter SPI mode
- decode commands
- return valid structured responses

but it remains stuck in idle during ACMD41 initialization.

This kind of card is one of the main reasons SD Necromancer exists.

---

# Project structure

The codebase has been refactored into smaller modules.

Current structure is roughly:

```text
src/
├── benchmark/
│   ├── bench_math.*
│   ├── benchmark.*
│   ├── benchmark_chunked_io.cpp
│   ├── benchmark_internal.h
│   ├── benchmark_raw.cpp
│   └── spi_scale_benchmark.cpp
│
├── core/
│   ├── app_types.h
│   └── config.h
│
├── hardware/
│   ├── card_format.*
│   ├── card_identity.*
│   ├── sd_card.*
│   ├── touch.*
│   └── touch_calibration.*
│
├── tools/
│   ├── sd_necromancer.*
│   └── sd_bitbang_probe.*
│
├── ui/
│   ├── display_diagnostics.cpp
│   ├── display_necromancer.cpp
│   ├── display_progress.cpp
│   ├── display_results.cpp
│   ├── display_ui.*
│   ├── display_ui_internal.*
│   └── ui_geometry.*
│
└── main.cpp
```

---

# Host-side tests

Several pure-logic modules can be tested outside the ESP32 target.

Existing test areas include:

```text
test_app_types
test_bench_math
test_card_format
test_card_identity
test_touch_calibration
test_ui_geometry
```

Pure logic is intentionally separated from hardware-dependent code where practical.

---

# Code quality

The project has been through substantial cleanup/refactoring.

Areas improved include:

- reduced duplication
- smaller modules
- pure benchmark math
- isolated touch calibration
- isolated UI geometry
- reduced `String` usage
- benchmark/UI separation
- explicit card state structures
- reduced hidden UI state
- DMA-safe benchmark buffers

Hardware stability and real card behavior remain more important than chasing a static-analysis score.

---

# Planned features

Likely future tools include:

## Partition inspector

- MBR parser
- GPT parser
- partition list
- filesystem signature detection

Possible signatures:

- FAT12
- FAT16
- FAT32
- exFAT
- NTFS
- ext2
- ext3
- ext4

---

## Raw sector viewer

Planned:

- sector number selector
- hexadecimal dump
- ASCII view
- jump to sector
- MBR/GPT shortcuts
- boot-sector shortcuts

---

## Formatting

Possible future support:

- FAT formatting
- FAT32 formatting
- exFAT formatting

Formatting will remain separate from normal diagnostics because it is destructive.

---

## Repartitioning

Possible future partition operations:

- wipe partition table
- create new MBR
- create partition
- recreate filesystem

These operations will require explicit warnings.

---

## Full-capacity verification

A proper fake-capacity test is planned.

This would:

1. write unique patterns across the card
2. read the entire address space back
3. verify that data remains at the correct addresses
4. detect wraparound/aliasing
5. report actual reliable capacity

This test would be destructive.

---

## Network features

Possible later additions:

- Wi-Fi status page
- browser-based diagnostics
- download raw sectors
- upload/download card files
- remote benchmark results

---

# USB note

The classic ESP32 used by this CYD does **not** provide native USB OTG support.

Therefore the board cannot simply become a proper USB Mass Storage device without additional USB hardware.

An ESP32-S3-based board would be a better target for native USB MSC functionality.

---

# Development philosophy

The project follows a few practical rules:

1. A successful mount is not the same thing as a healthy card.
2. A successful `cardBegin()` is not sufficient proof that reads actually work.
3. CID is useful metadata, not authenticity proof.
4. Benchmarks must avoid UI work inside timed sections.
5. Experimental raw recovery belongs behind explicit warnings.
6. Destructive tests should preserve and restore original data when possible.
7. Real hardware behavior matters more than theoretical cleanliness.
8. Weird cards are useful test equipment. 🙂

---

# Warning

This project includes experimental low-level and destructive SD-card operations.

Use normal **SCAN**, **BENCH** and **DIAG** modes for ordinary cards.

Use **SD NECROMANCER** only on cards whose data can be lost.

There is no guarantee that a damaged, counterfeit or failing card can be recovered, and some operations can make an already damaged filesystem unreadable.

---

# License

No license has been selected yet.

If this project is going to be published publicly, add an explicit license before distribution.

