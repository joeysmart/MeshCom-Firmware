# Raspberry Pi native SX127X target

This builds a Linux-native, receive-only SX1276 application using RadioLib
with Portduino's Linux SPI and GPIO layers. It reports received packets as
hexadecimal bytes. It is not yet the full MeshCom application: MeshCom's main
runtime and scheduler currently depend on the ESP32/nRF52 platform
implementations.

Like Meshtastic's native target, this uses the Portduino framework on
Meshtastic's pinned `platform-native` package. It runs directly on Raspberry
Pi Linux and does not use a build container. `pioarduino/platform-espressif32`
targets ESP32 microcontrollers and cannot produce a Linux Raspberry Pi
executable.

## Radio wiring

Defaults in `configuration.h` use Raspberry Pi BCM GPIO numbering and SPI0
CE0:

| SX127X | Raspberry Pi |
| --- | --- |
| NSS / CS | BCM 8 (SPI0 CE0) |
| SCK | BCM 11 |
| MOSI | BCM 10 |
| MISO | BCM 9 |
| DIO0 | BCM 25 |
| DIO1 | BCM 24 |
| RESET | BCM 22 |
| VCC | 3.3 V |
| GND | GND |

These are configurable defaults, not a claim that every SX127X HAT uses this
pinout. Update `LORA_*` and `RPI_SPI_*` in `configuration.h` to match the
specific radio board. Do not connect a 5 V radio signal to Raspberry Pi GPIO.

Enable SPI on the Pi and install the `libgpiod` development library before
building (for Raspberry Pi OS, `sudo apt install libgpiod-dev`).

## Build and run the radio probe

From the firmware repository root:

```sh
pio run -e raspberry_pi
.pio/build/raspberry_pi/program
```

The target binds DIO0, DIO1, and reset through Portduino's Linux GPIO layer,
opens the configured spidev device, and initializes the SX1276 with MeshCom's
default LoRa settings. It does not transmit; received packets are printed as
hexadecimal bytes.
