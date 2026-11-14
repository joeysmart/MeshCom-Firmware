#include <cstdio>

#include <RadioLib.h>
#include <hal/RPi/PiHal.h>

#include "configuration.h"

static PiHal radioHal(RPI_SPI_CHANNEL, RPI_SPI_SPEED, RPI_SPI_DEVICE, RPI_GPIO_DEVICE);
static SX1276 radio = new Module(&radioHal, LORA_CS, LORA_DIO0, LORA_RST, LORA_DIO1);

int main()
{
    std::printf("[PiRadio] Initializing SX1276 ... ");
    const int16_t state = radio.begin(
        RF_FREQUENCY,
        LORA_BANDWIDTH,
        LORA_SF,
        LORA_CR,
        SYNC_WORD_SX127x,
        TX_OUTPUT_POWER,
        LORA_PREAMBLE_LENGTH);

    if (state != RADIOLIB_ERR_NONE)
    {
        std::fprintf(stderr, "failed, RadioLib status %d\n", state);
        return 1;
    }

    std::puts("success");
    const int16_t sleepState = radio.sleep();
    if (sleepState != RADIOLIB_ERR_NONE)
    {
        std::fprintf(stderr, "[PiRadio] Radio sleep failed, status %d\n", sleepState);
        return 1;
    }

    return 0;
}
