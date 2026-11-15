#include <atomic>
#include <cstddef>
#include <cstdint>

#include <RadioLib.h>

#include "configuration.h"

#include <linux/gpio/LinuxGPIOPin.h>

static SX1276 radio = new Module(RADIOLIB_NC, LORA_DIO0, LORA_RST, LORA_DIO1);
static std::atomic<bool> packetReceived{false};

void portduinoSetup()
{
    gpioBind(new LinuxGPIOPin(LORA_DIO0, RPI_GPIO_CHIP, LORA_DIO0, "LoRa DIO0"));
    gpioBind(new LinuxGPIOPin(LORA_DIO1, RPI_GPIO_CHIP, LORA_DIO1, "LoRa DIO1"));
    gpioBind(new LinuxGPIOPin(LORA_RST, RPI_GPIO_CHIP, LORA_RST, "LoRa reset"));
    SPI.begin(RPI_SPI_PATH, RPI_SPI_SPEED);
}

static void onPacketReceived()
{
    packetReceived.store(true, std::memory_order_release);
}

void setup()
{
    Serial.begin(115200);
    Serial.println("[PiRadio] Initializing SX1276");

    int16_t state = radio.begin(
        RF_FREQUENCY,
        LORA_BANDWIDTH,
        LORA_SF,
        LORA_CR,
        SYNC_WORD_SX127x,
        TX_OUTPUT_POWER,
        LORA_PREAMBLE_LENGTH);

    if (state != RADIOLIB_ERR_NONE)
    {
        Serial.printf("[PiRadio] Initialization failed, RadioLib status %d\n", state);
        return;
    }

    radio.setPacketReceivedAction(onPacketReceived);
    state = radio.startReceive();
    if (state != RADIOLIB_ERR_NONE)
    {
        Serial.printf("[PiRadio] Receive start failed, RadioLib status %d\n", state);
        return;
    }

    Serial.println("[PiRadio] Listening for LoRa packets");
}

void loop()
{
    if (!packetReceived.exchange(false, std::memory_order_acquire))
    {
        delay(1);
        return;
    }

    uint8_t payload[256];
    const size_t size = radio.getPacketLength();
    int16_t state = RADIOLIB_ERR_NONE;
    if (size > sizeof(payload))
    {
        Serial.printf("[PiRadio] Packet too large: %u\n", static_cast<unsigned>(size));
        state = RADIOLIB_ERR_PACKET_TOO_LONG;
    }
    else
    {
        state = radio.readData(payload, size);
        if (state == RADIOLIB_ERR_NONE)
        {
            Serial.printf("[PiRadio] RX len=%u RSSI=%d SNR=%d payload=",
                          static_cast<unsigned>(size), radio.getRSSI(), radio.getSNR());
            for (size_t i = 0; i < size; ++i)
                Serial.printf("%02X", payload[i]);
            Serial.println();
        }
        else
        {
            Serial.printf("[PiRadio] Read failed, RadioLib status %d\n", state);
        }
    }

    state = radio.startReceive();
    if (state != RADIOLIB_ERR_NONE)
        Serial.printf("[PiRadio] Receive restart failed, RadioLib status %d\n", state);
}
