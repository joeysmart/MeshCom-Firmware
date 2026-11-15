#include <Arduino.h>
#include <RadioLib.h>
#include <SPI.h>

#include <blocked_calls.h>
#include <capture_functions.h>
#include <configuration.h>
#include <extudp_functions.h>
#include <loop_functions_extern.h>
#include <loop_scheduler.h>
#include <lora_functions.h>
#include <lora_setchip.h>
#include <loop_functions.h>
#include <nbr_matrix.h>
#include <nbr_views.h>
#include <rm_runtime.h>
#include <serial_command.h>
#include <time_functions.h>
#include <txring_functions.h>
#include <uptime_min.h>

#include <linux/gpio/LinuxGPIOPin.h>

#include "rpi_storage.h"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstdlib>

bool rpiFlashReady(void);

SX1276 radio = new Module(LORA_CS, LORA_DIO0, LORA_RST, LORA_DIO1);

int transmissionState = RADIOLIB_ERR_UNKNOWN;
unsigned long iReceiveTimeOutTime = 0;
unsigned long retransmit_timer = 0;

namespace
{
std::atomic<bool> receiveFlag{false};
std::atomic<bool> transmittedFlag{false};
std::atomic<bool> bEnableInterruptReceive{false};
std::atomic<bool> bEnableInterruptTransmit{false};

void setFlagReceive()
{
    if (bEnableInterruptReceive)
        receiveFlag = true;
    if (bEnableInterruptTransmit)
        transmittedFlag = true;
}

void setFlagSent()
{
    if (bEnableInterruptReceive)
        receiveFlag = true;
    if (bEnableInterruptTransmit)
        transmittedFlag = true;
}

[[noreturn]] void stopWithError(const char *operation, int error)
{
    Serial.printf("[BOOT];fatal;%s;error;%d\n", operation, error);
    std::exit(EXIT_FAILURE);
}

int startReceiver()
{
    bEnableInterruptTransmit = false;
    bEnableInterruptReceive = false;
    radio.clearPacketSentAction();
    radio.clearPacketReceivedAction();
    radio.setPacketReceivedAction(setFlagReceive);
    bEnableInterruptReceive = true;
    const int result = radio.startReceive();
    if (result != RADIOLIB_ERR_NONE)
    {
        bEnableInterruptReceive = false;
        radio.clearPacketReceivedAction();
        bRadio = false;
        Serial.printf("[LoRa];rx_start_failed;error;%d\n", result);
        return result;
    }
    is_receiving = false;
    return result;
}

void handleReceivedPacket()
{
    uint8_t payload[256];
    const size_t packetLength = radio.getPacketLength();
    const size_t readLength = std::min(packetLength, sizeof(payload));

    bEnableInterruptReceive = false;
    radio.clearPacketReceivedAction();
    const int result = radio.readData(payload, readLength);
    if (result == RADIOLIB_ERR_NONE && packetLength <= sizeof(payload))
    {
        OnRxDone(payload, static_cast<uint16_t>(packetLength),
                 static_cast<int16_t>(radio.getRSSI()),
                 static_cast<int8_t>(radio.getSNR()));
    }
    else
    {
        Serial.printf("[LoRa];rx_read_failed;error;%d;length;%u\n",
                      result, static_cast<unsigned>(packetLength));
        OnRxError();
    }

    startReceiver();
    iReceiveTimeOutTime = millis();
    csma_reset();
}

void handleTransmitComplete()
{
    bEnableInterruptTransmit = false;
    radio.clearPacketSentAction();
    if (transmissionState == RADIOLIB_ERR_NONE)
        OnTxDone();
    else
        OnTxTimeout();

    if (bSetLoRaAPRS && lora_setchip_meshcom())
        bSetLoRaAPRS = false;

    startReceiver();
    iReceiveTimeOutTime = millis();
    csma_reset();
}

void restoreReceiveAfterCad()
{
    startReceiver();
    iReceiveTimeOutTime = millis();
}

void serviceRadio()
{
    if (!bRadio)
        return;

    if (receiveFlag.exchange(false))
        handleReceivedPacket();

    if (transmittedFlag.exchange(false))
        handleTransmitComplete();

    if (bEnableInterruptTransmit || tx_is_active)
        return;

    const unsigned long now = millis();
    if (iReceiveTimeOutTime != 0)
    {
        if (static_cast<uint32_t>(now - iReceiveTimeOutTime) < csma_timeout)
            return;
        iReceiveTimeOutTime = 0;
    }

    const int nextSlot = getNextTxSlot();
    if (nextSlot < 0)
    {
        iReceiveTimeOutTime = now;
        return;
    }

    bEnableInterruptReceive = false;
    radio.clearPacketReceivedAction();
    const int cadResult = radio.scanChannel();
    if (cadResult != RADIOLIB_CHANNEL_FREE)
    {
        if (cadResult < 0)
            Serial.printf("[LoRa];cad_failed;error;%d\n", cadResult);
        else
        {
            ++cad_attempt;
            csma_timeout = csma_compute_timeout_nofloor(cad_attempt);
        }
        restoreReceiveAfterCad();
        return;
    }

    tx_cad_attempt = cad_attempt;
    csma_reset();
    bEnableInterruptTransmit = true;
    radio.setPacketSentAction(setFlagSent);
    if (!doTX())
    {
        bEnableInterruptTransmit = false;
        radio.clearPacketSentAction();
        restoreReceiveAfterCad();
    }
}

void loadRuntimeSettings()
{
    bDEBUG = (meshcom_settings.node_sset & 0x0008) != 0;
    bLORADEBUG = (meshcom_settings.node_sset & 0x0200) != 0;
    bSHORTPATH = (meshcom_settings.node_sset & 0x0400) != 0;
    bGATEWAY = false;
    bEXTUDP = (meshcom_settings.node_sset & 0x2000) != 0;
    bWEBSERVER = false;
    bNETCONSOLE = false;
    bMESH = (meshcom_settings.node_sset2 & 0x0020) == 0;
    bVIA = (meshcom_settings.node_sset2 & 0x4000) != 0;
    bNoMSGtoALL = (meshcom_settings.node_sset3 & 0x0002) != 0;
    posinfo_interval = meshcom_settings.node_postime > 0
        ? static_cast<unsigned long>(meshcom_settings.node_postime)
        : static_cast<unsigned long>(POSINFO_INTERVAL);

    if (meshcom_settings.node_country == 0)
        meshcom_settings.node_country = 8;
}

void serviceMeshAnnouncements()
{
    if (!bRadio || isNodeUnconfigured(meshcom_settings.node_call))
        return;

    const unsigned long now = millis();
    const int neighbors = nbrNcnt(nbrMatrix, uptimeMin16());
    static bool firstHey = true;

    if (firstHey || static_cast<uint32_t>(now - heyinfo_timer) >= trickle_interval_ms)
    {
        if (trickle_last_neighbor_count >= 0 && neighbors != trickle_last_neighbor_count)
        {
            trickle_interval_ms = TRICKLE_IMIN_S * 1000UL;
            trickle_consistent_count = 0;
        }
        trickle_last_neighbor_count = neighbors;

        if (!heyTrickleSuppress(trickle_consistent_count))
            sendHey();

        trickle_interval_ms = std::min(trickle_interval_ms * 2,
                                       static_cast<unsigned long>(TRICKLE_IMAX_S) * 1000UL);
        trickle_consistent_count = 0;
        heyinfo_timer = millis();
        firstHey = false;
    }

    nbrReportTick();

    const bool positionDue =
        static_cast<uint32_t>(now - posinfo_timer) >= posinfo_interval * 1000UL ||
        posinfo_shot;
    if (positionDue &&
        static_cast<uint32_t>(now - posinfo_timer_min) >= 30000UL)
    {
        if (meshcom_settings.node_lat != 0.0 || meshcom_settings.node_lon != 0.0)
        {
            sendPosition(posinfo_interval,
                         meshcom_settings.node_lat, meshcom_settings.node_lat_c,
                         meshcom_settings.node_lon, meshcom_settings.node_lon_c,
                         meshcom_settings.node_alt, meshcom_settings.node_press,
                         meshcom_settings.node_hum, meshcom_settings.node_temp,
                         meshcom_settings.node_temp2, meshcom_settings.node_gas_res,
                         meshcom_settings.node_co2, meshcom_settings.node_press_alt,
                         meshcom_settings.node_press_asl);
        }
        posinfo_shot = false;
        posinfo_timer = millis();
        posinfo_timer_min = millis();
    }
    else if (!positionDue)
    {
        posinfo_timer_min = now;
    }
}
}

void portduinoSetup()
{
    gpioBind(new LinuxGPIOPin(LORA_DIO0, RPI_GPIO_CHIP, LORA_DIO0, "LoRa DIO0"));
    gpioBind(new LinuxGPIOPin(LORA_DIO1, RPI_GPIO_CHIP, LORA_DIO1, "LoRa DIO1"));
    gpioBind(new LinuxGPIOPin(LORA_RST, RPI_GPIO_CHIP, LORA_RST, "LoRa reset"));
}

void raspberryPiSetup()
{
    Serial.begin(115200);
    Serial.setTimeout(50);
    Serial.println("[BOOT];platform;raspberry_pi_native");

    if (!RaspberryPiStorage::initialize())
        stopWithError("storage", errno);

    init_loop_function();
    init_flash();
    if (!rpiFlashReady())
        stopWithError("settings_load", errno);
    loadTimePersistence();
    loadRuntimeSettings();
    blockListLoad();
    rmInit();

    SPI.begin(RPI_SPI_PATH, RPI_SPI_SPEED);

    const int beginResult = radio.begin(getFreq());
    if (beginResult != RADIOLIB_ERR_NONE)
        stopWithError("radio_begin", beginResult);

    lora_setcountry(meshcom_settings.node_country);
    if (!lora_setchip_meshcom())
        stopWithError("radio_config", RADIOLIB_ERR_UNKNOWN);

    bRadio = true;
    const int receiveResult = startReceiver();
    if (receiveResult != RADIOLIB_ERR_NONE)
        stopWithError("radio_receive", receiveResult);

    iReceiveTimeOutTime = millis();
    save_settings();
    Serial.printf("[BOOT];ready;ms;%lu;radio;1\n", static_cast<unsigned long>(millis()));
}

void raspberryPiLoop()
{
    checkSerialCommand();
    loopSchedulerRun(millis());
    bpPollDrain();
    rmDrain();
    serviceMeshAnnouncements();

    static unsigned long lastTimeSave = 0;
    if (static_cast<uint32_t>(millis() - lastTimeSave) >= 900000UL)
    {
        saveTimePersistence();
        lastTimeSave = millis();
    }

    if (bEXTUDP)
    {
        static unsigned long externRetry = 0;
        if (!hasExternIPaddress &&
            (externRetry == 0 || static_cast<uint32_t>(millis() - externRetry) >= 5000))
        {
            externRetry = millis();
            startExternUDP();
        }
        getExternUDP();
        flushExternQueue();
    }
    serviceRadio();
    delay(1);
}
