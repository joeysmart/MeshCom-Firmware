# Raspberry Pi native SX127x target

This target runs the MeshCom firmware as a Linux process on a Raspberry Pi.
It uses Portduino for the Arduino APIs, RadioLib for the SX127x radio, and
POSIX UDP sockets for the external JSON-UDP interface.

## Requirements

- Raspberry Pi OS with SPI enabled (`/dev/spidev0.0` available)
- `libgpiod-dev` and `libi2c-dev`
- GPIO access for the user running the firmware
- PlatformIO Core

The target uses the project-owned `linux_arm` board definition and the pinned
Portduino framework from `platform-native`.

## Radio wiring

The default pin mapping in `configuration.h` uses BCM numbering and SPI0 CE0:

| SX127x | Raspberry Pi |
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

These are configurable defaults, not a claim that every SX127x HAT uses this
pinout. Update `LORA_*` and `RPI_SPI_*` in `configuration.h` for the attached
radio. Do not connect 5 V signals to Raspberry Pi GPIO.

## Build and run

From the firmware repository root:

```sh
pio run -e raspberry_pi
.pio/build/raspberry_pi/program
```

Settings, counters, and block-list data are stored under
`$HOME/.meshcom/raspberry_pi` by default. Set `MESHCOM_DATA_DIR` to use another
directory.

The serial console accepts the MeshCom command syntax. External JSON-UDP can
be enabled with `--extudp on` and configured with `--extudpip <host-or-ip>`;
the firmware listens on UDP port 1799 and sends frames to that peer.

The native target provides the local SX127x mesh RX/TX path, command console,
persistent settings, and external JSON-UDP. The ESP32 web UI, BLE, displays,
physical buttons, GPS, sensors, KISS TCP, and hardware battery readings are
not available on this target. Network interface addressing is managed by
Linux.
