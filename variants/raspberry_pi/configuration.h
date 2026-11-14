#pragma once

#include <configuration_global.h>

#define MODUL_HARDWARE RASPBERRY_PI_NATIVE
#define SX127X

#define LORA_CS 8
#define LORA_DIO0 25
#define LORA_DIO1 24
#define LORA_RST 22

#define RPI_SPI_DEVICE 0
#define RPI_SPI_CHANNEL 0
#define RPI_SPI_SPEED 2000000
#define RPI_GPIO_DEVICE 0

#define TX_POWER_MAX 17
#define TX_POWER_MIN 2
#define TX_OUTPUT_POWER 17

#include <configuration_default.h>
