#pragma once

#include <configuration_global.h>

#define MODUL_HARDWARE RASPBERRY_PI_NATIVE
#define SX127X

#define LORA_CS -1
#define LORA_DIO0 25
#define LORA_DIO1 24
#define LORA_RST 22

#define RPI_SPI_PATH "/dev/spidev0.0"
#define RPI_SPI_SPEED 2000000
#define RPI_GPIO_CHIP "gpiochip0"

#define TX_POWER_MAX 17
#define TX_POWER_MIN 2
#define TX_OUTPUT_POWER 17

#define ENABLE_GPS_DISABLED
#define ENABLE_BMX280_DISABLED
#define ENABLE_BMX680_DISABLED
#define ENABLE_RTC_DISABLED
#define ENABLE_BMP390_DISABLED
#define ENABLE_AHT20_DISABLED
#define ENABLE_MCP23017_DISABLED
#define ENABLE_SHT21_DISABLED
#define ENABLE_MC811_DISABLED
#define ENABLE_INA226_DISABLED
#define DISABLE_NET_CONSOLE

#include <configuration_default.h>
