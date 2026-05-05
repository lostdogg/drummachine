# Pico Drum Machine

A 16-step, 4-track drum machine running on a Raspberry Pi Pico.  
Audio output is driven by a PCM5102A DAC over I²S via PIO.  
Potentiometers are read from an ADS1115 ADC over I²C.  
A CYD (2432S028) display shares the same I²C bus.

---

## Hardware Wiring

### 1. Power

| Connection | Detail |
|---|---|
| **Common GND** | Connect GND of Pico, CYD, ADS1115, and PCM5102A together |
| **5 V rail** | Pico VBUS (Physical Pin 40) → CYD VIN and PCM5102A VIN |
| **3.3 V rail** | Pico 3V3(OUT) (Physical Pin 36) → ADS1115 VDD |
| **PCM5102A SCK** | Tie to GND (enables internal clock generation) |

---

### 2. I²C Bus (i2c0) — Pico as Master

| Function | Pico GPIO (Physical Pin) | CYD Pin | ADS1115 Pin |
|---|---|---|---|
| SDA (data) | GPIO 4 (Pin 6) | GPIO 21 | SDA |
| SCL (clock) | GPIO 5 (Pin 7) | GPIO 22 | SCL |

* **ADS1115 ADDR** → GND (I²C address **0x48**)  
* Internal pull-ups are enabled in firmware; add external 4.7 kΩ resistors to 3.3 V if communication is unreliable.

---

### 3. Audio Output — I²S via PIO → PCM5102A

| Signal | Pico GPIO (Physical Pin) | PCM5102A Pin |
|---|---|---|
| BCK (bit clock) | GPIO 26 (Pin 31) | BCK |
| LRCK (word select) | GPIO 27 (Pin 32) | LRCK |
| DIN (data) | GPIO 28 (Pin 34) | DIN |

---

### 4. Potentiometers (10 kΩ) — via ADS1115

Connect outer pot legs to 3.3 V and GND; wiper to the corresponding ADS1115 input.

| Pot | ADS1115 Input | Function |
|---|---|---|
| Pot 1 | A0 | Tempo (BPM) |
| Pot 2 | A1 | Master Volume |
| Pot 3 | A2 | Pattern Bank |
| Pot 4 | A3 | Pitch / Filter |

---

### 5. Button

| Function | Pico GPIO (Physical Pin) | Wiring |
|---|---|---|
| Trigger / Mode | GPIO 15 (Pin 20) | Button to GND; internal pull-up enabled in firmware |

---

## Sequencer Overview

* **4 tracks**: Kick, Snare, Hi-Hat, Clap  
* **16 steps** per bar  
* **4 pattern banks** selectable via Pot 3  
* Voices are synthesised on-chip (no external sample flash required)  
* Press the button to **start / stop** playback

---

## Building

Prerequisites: [Raspberry Pi Pico C/C++ SDK](https://github.com/raspberrypi/pico-sdk)

```bash
export PICO_SDK_PATH=/path/to/pico-sdk

mkdir build && cd build
cmake ..
make -j$(nproc)
```

Flash `drummachine.uf2` to the Pico by holding BOOTSEL while connecting USB,
then copying the file to the mass-storage drive that appears.

Serial output (printf) is routed over USB CDC; open at any baud rate to see
status messages.