# CYD SD Tester v0.7

Cleanup/refactor release for the ESP32-2432S028 / ESP32-2432S028R family.

## Main changes

### Benchmark UI

The old separate BENCH and SCALE buttons are now one BENCH menu:

```text
BENCHMARK
- Quick Benchmark
- SPI Scaling
- Back
```

Quick benchmark results use horizontal bars.

SPI scaling results use a small line graph.

### DMA-safe qualification

The normal SD qualification path now uses an internal DMA-capable 512-byte buffer
for the real sector-read qualification step.

This means 40 MHz is tested using the same type of DMA-safe memory that produced the
large performance improvement in v0.6.1.

### SPI scaling moved to benchmark.cpp

`sd_card.cpp` is now limited to:

- SPI/card initialization
- speed qualification
- CID/CSD
- filesystem mount
- card identity/state

Benchmark logic lives in `benchmark.cpp`.

### Dead experimental code removed

Removed:

- raw chunk sweep settings
- best chunk result fields
- shared/dedicated A/B switches
- experimental yield toggles
- raw-progress experimental toggle

The known-good performance path remains:

```text
DEDICATED_SPI
USE_SPI_ARRAY_TRANSFER=1
DMA-capable internal buffers
no yield() inside transfer loops
```

### UI cleanup

- Removed hidden `lastTitle` progress-screen state.
- Progress screens start explicitly when `done == 0`.
- Reduced temporary Arduino `String` usage.
- Version number remains visible after each screen title.

### Identity cleanup

The old `filesystemBytes` naming was misleading because it was derived from filesystem
cluster geometry rather than the full partition size.

It is now named:

```text
filesystemDataBytes
```

The questionable "filesystem exceeds card" fake-card check has been removed.

A proper card/partition/filesystem size consistency check will be added once MBR/GPT
and boot-sector parsing exist.

## Benchmark behavior

Quick benchmark:

- 16 MiB preallocated file write
- 16 MiB file read
- separate CRC verification
- 16 MiB raw read

SPI scaling:

- 40 MHz
- 20 MHz
- 10 MHz
- 4 MHz
- 1 MHz

The scaling test restores normal card qualification and filesystem mount when finished.

## Safety

Current tests are non-destructive except for temporary creation/removal of:

```text
/SDTEST.BIN
```

Raw tests are read-only.

## v0.7.1

Normal SD qualification no longer attempts 40 MHz.

Normal scan/fallback now uses:

```text
20 MHz
10 MHz
4 MHz
1 MHz
```

The SPI Scaling benchmark still intentionally tests:

```text
40 MHz
20 MHz
10 MHz
4 MHz
1 MHz
```

This keeps 40 MHz available as a diagnostic/experimental data point without
slowing every normal card scan with a known-unreliable qualification attempt.
