# Circuit — Rev 5.0 (corrected)
### INMP441 + ESP32-S3 HW678 DevKit — SIH 2026

Supersedes Rev 4.0. Every change below exists because something in Rev 4.0 was wrong or because your last recording proved the old layout was not delivering audio-band signal.

---

## What changed from Rev 4.0, and why

| # | Rev 4.0 said | Rev 5.0 says | Why |
|---|---|---|---|
| 1 | 100 nF decoupling only | **100 nF + 10 µF bulk** | The onboard WS2812B pulses tens of mA on the same 3V3 rail. 100 nF alone cannot supply that; the sag lands in your audio as a periodic tick. |
| 2 | Any jumper length | **≤ 10 cm, SCK not adjacent to SD** | BCLK runs at 1.024 MHz. Long parallel jumpers crosstalk, which is a prime suspect for your sub-20 Hz garbage capture. |
| 3 | Nothing about ground layout | **Star ground at one board GND pin** | Daisy-chained grounds on a breadboard build up a shared impedance that shows up as low-frequency drift. |
| 4 | "GPIO 2 = built-in LED" (Test 1) | **There is no LED on GPIO 2** | Your board's only LED is the WS2812B on GPIO 48. That checklist item could never pass. |
| 5 | Pinout diagram with IO1 three times | **Diagram deleted — use the silkscreen** | The Rev 4.0 ASCII pinout was internally inconsistent and contradicted its own "no external wiring" note. |
| 6 | L/R → GND, `ONLY_LEFT` in code | **L/R → GND, slot auto-probed in firmware** | The ESP32 I2S peripheral sometimes latches the *right* slot even with L/R grounded. Rev 5.0 firmware tests both and picks the live one. |

---

## Bill of Materials

| # | Component | Spec | Qty | Note |
|---|---|---|---|---|
| 1 | ESP32-S3 DevKit (HW678) | ESP32-S3-WROOM, 8 MB PSRAM | 1 | Program via the **left USB-C (COM)** port |
| 2 | INMP441 breakout | I2S MEMS, 3.3 V, −26 dBFS | 1 | |
| 3 | Ceramic cap 100 nF | any package | 1 | High-frequency decoupling |
| 4 | **Electrolytic/ceramic cap 10 µF** | ≥ 6.3 V | 1 | **NEW** — bulk reservoir |
| 5 | Breadboard | 830 pt | 1 | |
| 6 | Jumper wires M-M | **short, ≤ 10 cm** | 6 | Length matters here |
| 7 | USB-C cable | **data-capable** | 1 | Charge-only cables give no COM port |

No external LED, no resistor — the WS2812B is onboard on GPIO 48.

---

## Wiring table

### Power
| From | To | Colour | Note |
|---|---|---|---|
| ESP32-S3 `3V3` | INMP441 `VDD` | 🔴 Red | **3.3 V only.** 5 V destroys the mic. |
| ESP32-S3 `GND` | INMP441 `GND` | ⚫ Black | Run this **directly** to a board GND pin, not chained off another component's ground |
| ESP32-S3 `GND` | INMP441 `L/R` | ⚫ Black | Must be firm — a floating L/R is the single most common cause of a dead-looking mic |
| INMP441 `VDD` ↔ `GND` | 100 nF | — | **Within 5 mm of the VDD pin** |
| INMP441 `VDD` ↔ `GND` | 10 µF | — | Same rail, can sit a few cm away. Watch polarity if electrolytic |

### I2S signals
| ESP32-S3 | INMP441 | Direction | Colour | Function |
|---|---|---|---|---|
| `GPIO 4` | `WS` | out → | 🟡 Yellow | Word select / LRCLK |
| `GPIO 5` | `SCK` | out → | 🟠 Orange | Bit clock / BCLK (1.024 MHz) |
| `GPIO 7` | `SD` | ← in | 🔵 Blue | Audio data from mic |

**These three GPIOs are verified safe on ESP32-S3:** not strapping pins (0, 3, 45, 46), not USB (19, 20), not flash/PSRAM (26–37). Keep them.

### LED
Nothing to wire. WS2812B is internal on **GPIO 48**.

---

## Physical layout rules (this is the part that likely fixes your recording)

1. **Keep SCK and SD on non-adjacent breadboard rows.** If you must run them near each other, put a **grounded jumper between them** as a shield.
2. **Shortest possible wires.** Under 10 cm. At 1.024 MHz, a 20 cm unshielded jumper behaves like an antenna.
3. **Star ground.** Mic GND and L/R GND both go to the *same* board GND pin, not chained through the breadboard rail across the whole board.
4. **Seat the mic firmly.** A marginal contact on SD produces exactly the slow-drifting, near-DC signal your last WAV showed.
5. **Don't drive the WS2812B while capturing a KWS window.** Update the LED between captures, not during.

---

## Bring-up order

| Step | File | Pass criteria |
|---|---|---|
| 1 | `01_mic_diagnostic.ino` (or `_LEGACY`) | Every line prints PASS. Speech-band energy **≥ 60%**, clipping **< 0.10%**, ZCR **≥ 200/s**, SNR **> 20 dB** |
| 2 | `02_wav_recorder.ino` + `capture_wav.py` | `out.wav` is intelligible and the script prints **RECORDING IS GOOD** |
| 3 | KWS pipeline | Only after steps 1 and 2 both pass |

---

## Troubleshooting (corrected)

| Symptom | Likely cause | Fix |
|---|---|---|
| Audio-band energy < 60%, mostly sub-20 Hz | Clock/bit misalignment or bad SD contact | Shorten jumpers, reseat SD, separate SCK from SD, re-run diagnostic |
| Flat silence regardless of sound | Wrong slot latched | Diagnostic auto-probes — check which slot it reports and set `USE_RIGHT_SLOT` accordingly |
| Peak always 0 | VDD or GND not connected | Check red and black wires |
| Values pinned at ±32768 constantly | SD floating | Confirm pull-down is applied **after** `i2s_set_pin` / `init_std_mode` (Rev 5.0 does this) |
| Clipping > 0.10% | `SOFT_GAIN` too high | Lower `SOFT_GAIN` from 4.0 toward 1.0 |
| Speech RMS below 1000 | Too quiet / too far | Raise `SOFT_GAIN`, or speak 10–15 cm from the mic |
| Periodic tick every 0.5 s | WS2812B current pulses | Add the 10 µF cap; don't update the LED mid-capture |
| Upload fails | Wrong port or CDC off | Use **left** USB-C, set `USB CDC On Boot = Enabled`, hold BOOT during power-on |
| Serial garbled | Baud mismatch | Diagnostic = **115200**, recorder = **921600** |

---

## Arduino IDE settings

```
Board:              ESP32S3 Dev Module
USB CDC On Boot:    Enabled
Flash Size:         8MB (or 16MB — match your board)
PSRAM:              OPI PSRAM
Partition Scheme:   Huge APP (needed later for TFLite Micro)
Upload Speed:       921600
Port:               the LEFT USB-C connector
```
