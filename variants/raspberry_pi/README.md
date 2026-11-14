# Raspberry Pi native SX127X foundation

This is an initial Linux-native radio target, not a runnable MeshCom firmware
port yet. It builds a small SX1276 bring-up program using RadioLib's Raspberry
Pi `PiHal`; the main MeshCom application still depends on its ESP32/nRF52
runtime and platform code.

The target uses PlatformIO's `native` platform because the executable runs on
the Raspberry Pi's Linux OS. `pioarduino/platform-espressif32` is for ESP32
microcontrollers and cannot produce a Linux Raspberry Pi executable.

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

Enable SPI on the Pi and install the `lgpio` development library before
building (for Raspberry Pi OS, `sudo apt install liblgpio-dev`).

## Build and run the radio probe

From the firmware repository root:

```sh
pio run -e raspberry_pi_radio
.pio/build/raspberry_pi_radio/program
```

The probe initializes the SX1276 using the MeshCom default LoRa parameters,
reports the RadioLib result, then puts the radio to sleep. It does not transmit
or start the MeshCom application.
