# ADC Voltmeter -- Running-Average Smoothing

Reads a voltage on PC5 (ADC5) and displays it in volts on a 4-digit 7-segment
display. The `adc` driver does the sampling, averaging, and millivolt
conversion -- `main.c` never touches ADMUX or ADCSRA directly.

## Hardware

- ATmega328P @ 1 MHz (internal oscillator, default fuses -- CKDIV8 active)
- 4-digit 7-segment display, 0.36 inch, THT (e.g. DM0026)
- Board: segments active HIGH, digit selects active HIGH (transistor-buffered)
- Voltage source (e.g. potentiometer wiper) on PC5, 0-5 V range
- 100 nF ceramic cap from PC5 to GND (cuts high-frequency noise)
- 220 ohm resistors on each segment pin (PD0-PD7)

## Wiring

| Signal          | MCU Pin | Notes                          |
|-----------------|---------|--------------------------------|
| Segment a       | PD7     |                                |
| Segment b       | PD6     |                                |
| Segment c       | PD5     |                                |
| Segment d       | PD4     |                                |
| Segment e       | PD3     |                                |
| Segment f       | PD2     |                                |
| Segment g       | PD1     |                                |
| Segment dp      | PD0     |                                |
| Digit 0 (ones)  | PC0     | active HIGH (NPN base)         |
| Digit 1 (tens)  | PC1     |                                |
| Digit 2 (hund)  | PC2     |                                |
| Digit 3 (thou)  | PC3     |                                |
| ADC input       | PC5     | 100 nF to GND for filtering    |

## Display Format

ADC result is converted to millivolts then shown with a decimal point after
the leftmost digit:

```
Input: 3.302 V  ->  ADC = 676  ->  averaged mv = 3302  ->  display: "3.302"
Input: 0.500 V  ->  ADC = 102  ->  averaged mv =  499  ->  display: "0.499"
```

## Why Use the ADC Driver

Two improvements over a hand-rolled `task_adc`:

### 1. Encapsulated configuration

`main.c` only sets a config struct:

```c
static const ADC_Config adc_cfg = {
    .channel   = 5,
    .ref_avcc  = 1,
    .prescaler = ADC_PRESCALER_DIV8,
    .avg_shift = 3,
};
```

No raw ADMUX/ADCSRA bit-twiddling, no warm-up boilerplate, no calculation of
prescaler magic numbers. `adc_init()` handles all of it.

### 2. Running average -- noise smoothing without lag

The driver maintains a running accumulator. Each `task_adc` call adds one
sample. Every `2^avg_shift` samples, the accumulator is divided by the same
power of 2 (a cheap right-shift, not a real divide) and published as the
"latest" averaged value:

```
sample 0: accumulator = ADC[0],            count = 1
sample 1: accumulator = ADC[0] + ADC[1],   count = 2
...
sample 7: accumulator = sum of 8 readings, count = 8
          latest = accumulator >> 3       <-- divide by 8 via shift
          accumulator = 0, count = 0      <-- start next window
```

Application code reads `adc_value()` (raw 0-1023) or `adc_mv(vref)` (millivolts).

## Smoothing Pipeline

```
ADC hardware
   |  conversion ~104 us @ 125 kHz ADC clock
   v
task_adc     (10 ms)  read sample, accumulate, average every 8 samples
   |
   v latest (averaged 0-1023, volatile uint16_t inside the driver)
task_display (50 ms)  reads latest, converts to mV, writes ssd4 buffer
   |
   v buf[4] inside ssd4 driver
task_ssd4    (1 ms)   multiplexes one digit per call (250 Hz refresh)
```

Each layer is independent. Changing one task's interval does not affect the others.

## Three Tuning Knobs

| Knob              | Where                                    | Effect                                            |
|-------------------|------------------------------------------|---------------------------------------------------|
| ADC prescaler     | `ADC_Config.prescaler`                   | Per-conversion accuracy (DIV8 = sweet spot at 1MHz) |
| Sample interval   | `tasks[]` array, `task_adc` interval     | How fast samples are accumulated                  |
| Averaging window  | `ADC_Config.avg_shift`                   | How many samples per averaged output (powers of 2) |

Current settings give 80 ms response time (10 ms x 8 samples).
For smoother readings: bump `avg_shift` from 3 to 4 (16 samples = 160 ms response).
For faster response: lower `avg_shift` to 2 (4 samples = 40 ms response).

## ADC Clock Prescaler

```
ADC clock = CPU clock / prescaler
```

The ATmega328P ADC needs a **50-200 kHz** clock for full 10-bit accuracy.
Outside that range, precision drops:

| Prescaler @ 1 MHz | ADC clock | Verdict                              |
|-------------------|-----------|--------------------------------------|
| DIV2              | 500 kHz   | Too fast -- drops to ~7-bit accuracy |
| DIV4              | 250 kHz   | Slightly too fast                    |
| **DIV8**          | **125 kHz** | **Within spec, full 10 bits**      |
| DIV16             | 62.5 kHz  | Within spec, slightly slower conv    |
| DIV128            | 7.8 kHz   | Below spec, accuracy drops           |

Conversion takes 13 ADC clocks (= 104 us at DIV8). With 10 ms sample
interval, the conversion is finished 96x before the next task tick -- plenty
of headroom.

## ADC Data Registers -- ADCL/ADCH and the Hardware Lock

After conversion completes, the 10-bit result is split across two registers
(right-aligned, ADLAR=0):

```
ADCH  =  [ 0 | 0 | 0 | 0 | 0 | 0 | bit9 | bit8 ]
ADCL  =  [ bit7 | bit6 | bit5 | bit4 | bit3 | bit2 | bit1 | bit0 ]
```

Once you read `ADCL`, the hardware freezes both registers until `ADCH` is
read. This prevents a torn read if a new conversion finishes between your
two register reads.

```c
// WRONG -- ADCH read first, no lock triggered
uint8_t hi = ADCH;
uint8_t lo = ADCL;

// CORRECT -- ADCL first, locks both until ADCH is read
uint8_t lo = ADCL;
uint8_t hi = ADCH;
uint16_t result = (hi << 8) | lo;
```

The `ADC` macro from `<avr/io.h>` handles the order automatically.
The driver uses `ADC` -- you never touch the individual byte registers.

## DIDR0 -- Why It Matters

`adc_init()` sets the bit in `DIDR0` for the selected channel. This disables
the **digital input buffer** on that pin. On an analog pin, the Schmitt
trigger constantly toggles between thinking the voltage is HIGH or LOW when
it sits near the threshold -- this wastes power and injects switching noise
back into the analog signal. Disabling it costs nothing and helps a little.

## Architecture

```
main.c
  ssd_cfg          <- display wiring (only thing to change when rewiring)
  adc_cfg          <- ADC channel + prescaler + averaging window
  task_display     <- reads averaged mV, writes to display buffer
  tasks[]          <- scheduler task array

src/drivers/
  adc.h / adc.c    <- ADC driver with running average
  ssd4.h / ssd4.c  <- 4-digit display driver
  scheduler.h/.c   <- cooperative task scheduler (uint32_t return = dynamic interval)
  millis.h/.c      <- Timer0 1 kHz tick, millis() for timestamps
```

## ADC Driver API

| Function | Description |
|---|---|
| `adc_init(cfg)` | Configure pin, ADMUX, ADCSRA, discard warm-up, start first conversion |
| `task_adc()` | Scheduler task -- one sample per call, averages over 2^avg_shift |
| `adc_value()` | Latest averaged raw value (0-1023) |
| `adc_mv(vref)` | Latest averaged value converted to millivolts |

## Build

```bash
make              # compile
make program      # flash via USBasp
make clean        # remove build artifacts
```

## Memory (ATmega328P @ 1 MHz)

| Region | Used   | Available | % Full |
|--------|--------|-----------|--------|
| Flash  | 1674 B | 32768 B   | 5.1%   |
| RAM    |   95 B |  2048 B   | 4.6%   |

## Requirements

- `avr-gcc`, `avrdude`, `avr-libc`
- USBasp programmer
- ATmega328P at 1 MHz (internal oscillator, default fuses)
- 220 ohm resistors on all 8 segment pins
- 100 nF ceramic cap from PC5 to GND (recommended for noise filtering)

---
Developed for PH_3032 Embedded Systems.
