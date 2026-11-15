#include <Arduino.h>
#include <configuration.h>
#include <configuration_global.h>
#include <counters_store.h>
#include <lora_setchip.h>
#include <maxhop.h>
#include <meshcom_settings.h>
#include <msgid_counter.h>
#include <settings_schema.h>
#include <settings_sanitize.h>
#include <settings_store.h>
#include <time_functions.h>
#include <clock.h>

#include "rpi_storage.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

unsigned long getUnixClock(void);

s_meshcom_settings meshcom_settings;

namespace
{
constexpr size_t kSettingsBufferSize = 8192;
bool s_flashInitialized = false;

void logSanitizedField(const char *field, const char *oldValue, const char *newValue)
{
    Serial.printf("[SETST];sanitize;%s;%s;%s\n", field, oldValue, newValue);
}

bool parseCounter(const char *name, uint32_t &value)
{
    std::string text;
    const auto result = RaspberryPiStorage::readFile(name, text);
    if (result == RaspberryPiStorage::ReadResult::missing)
        return false;
    if (result == RaspberryPiStorage::ReadResult::error)
    {
        Serial.printf("[SETST];counter;read_failed;%s\n", name);
        return false;
    }

    char *end = nullptr;
    errno = 0;
    const unsigned long parsed = std::strtoul(text.c_str(), &end, 10);
    if (errno != 0 || end == text.c_str() || parsed > UINT32_MAX)
    {
        Serial.printf("[SETST];counter;invalid;%s\n", name);
        return false;
    }
    while (*end == '\n' || *end == '\r' || *end == ' ' || *end == '\t')
        ++end;
    if (*end != '\0')
    {
        Serial.printf("[SETST];counter;invalid;%s\n", name);
        return false;
    }

    value = static_cast<uint32_t>(parsed);
    return true;
}

bool saveCounter(const char *name, uint32_t value)
{
    char text[16];
    const int len = std::snprintf(text, sizeof(text), "%u\n", value);
    return len > 0 && RaspberryPiStorage::writeFileAtomic(name, text, static_cast<size_t>(len));
}
}

void save_settings(void)
{
    char buffer[kSettingsBufferSize];
    const long size = settings_store::encode(settings_schema::fields(),
                                             settings_schema::fieldCount(),
                                             &meshcom_settings,
                                             buffer,
                                             sizeof(buffer));
    if (size < 0)
    {
        Serial.printf("[SETST];save;encode_failed;cap=%u\n",
                      static_cast<unsigned>(sizeof(buffer)));
        return;
    }
    if (!RaspberryPiStorage::writeFileAtomic("settings.kv", buffer, static_cast<size_t>(size)))
        Serial.printf("[SETST];save;write_failed;bytes=%ld\n", size);
}

bool rpiFlashReady(void)
{
    return s_flashInitialized;
}

void save_position(void)
{
    save_settings();
}

void init_flash(void)
{
    if (s_flashInitialized)
        return;
    if (!RaspberryPiStorage::initialize())
    {
        Serial.printf("[SETST];init;storage_failed\n");
        return;
    }

    meshcom_settings.node_call[0] = '\0';
    meshcom_settings.node_short[0] = '\0';
    meshcom_settings.node_lat_c = 'N';
    meshcom_settings.node_lon_c = 'E';
    meshcom_settings.max_hop_text = MAX_HOP_TEXT_DEFAULT;
    meshcom_settings.node_hamnet_only = 2;
    snprintf(meshcom_settings.node_gwsrv, sizeof(meshcom_settings.node_gwsrv), "OE");

    std::string contents;
    const auto result = RaspberryPiStorage::readFile("settings.kv", contents);
    if (result == RaspberryPiStorage::ReadResult::error)
    {
        Serial.printf("[SETST];load;read_failed\n");
        return;
    }
    else if (result == RaspberryPiStorage::ReadResult::ok)
    {
        const auto stats = settings_store::decode(settings_schema::fields(),
                                                   settings_schema::fieldCount(),
                                                   &meshcom_settings,
                                                   contents.data(),
                                                   contents.size());
        if (stats.malformed_lines != 0 || stats.unknown_keys != 0)
            Serial.printf("[SETST];load;malformed=%u;unknown=%u\n",
                          static_cast<unsigned>(stats.malformed_lines),
                          static_cast<unsigned>(stats.unknown_keys));
    }

    RadioParams params = {
        meshcom_settings.node_power,
        meshcom_settings.node_freq,
        meshcom_settings.node_bw,
        meshcom_settings.node_sf,
        meshcom_settings.node_cr,
        meshcom_settings.node_country
    };
    const RadioLimits limits = {
        TX_POWER_MIN, TX_POWER_MAX, 400.0f, 960.0f, 0, 0, max_country
    };
    const int correctedRadioFields =
        sanitize_radio_params(params, limits, logSanitizedField);
    meshcom_settings.node_power = params.power;
    meshcom_settings.node_freq = params.freq;
    meshcom_settings.node_bw = params.bw;
    meshcom_settings.node_sf = params.sf;
    meshcom_settings.node_cr = params.cr;
    meshcom_settings.node_country = params.country;

    const bool correctedMaxHop =
        sanitize_max_hop_text(meshcom_settings.max_hop_text, logSanitizedField);

    countersLoad();
    s_flashInitialized = true;

    if (correctedRadioFields != 0 || correctedMaxHop)
        save_settings();
}

void clear_flash(void)
{
    meshcom_settings = s_meshcom_settings{};
    meshcom_settings.node_call[0] = '\0';
    meshcom_settings.node_short[0] = '\0';
    meshcom_settings.node_lat_c = 'N';
    meshcom_settings.node_lon_c = 'E';
    meshcom_settings.max_hop_text = MAX_HOP_TEXT_DEFAULT;
    meshcom_settings.node_hamnet_only = 2;
    snprintf(meshcom_settings.node_gwsrv, sizeof(meshcom_settings.node_gwsrv), "OE");
    if (!RaspberryPiStorage::removeFile("settings.kv"))
        Serial.printf("[SETST];clear;remove_failed\n");
    s_flashInitialized = false;
    save_settings();
}

void countersLoad(void)
{
    uint32_t stored = static_cast<uint32_t>(meshcom_settings.node_msgid);
    parseCounter("node_msgid", stored);
    meshcom_settings.node_msgid = msgIdAfterLoad(static_cast<int>(stored));
    if (!countersSave())
        Serial.printf("[SETST];counter;load_writeback_failed\n");
}

bool countersSave(void)
{
    return saveCounter("node_msgid", static_cast<uint32_t>(meshcom_settings.node_msgid));
}

uint32_t rmHwmLoad(void)
{
    uint32_t value = 0;
    parseCounter("rm_hwm", value);
    return value;
}

bool rmHwmSave(uint32_t value)
{
    return saveCounter("rm_hwm", value);
}

uint32_t rmSndLoad(void)
{
    uint32_t value = 0;
    parseCounter("rm_snd", value);
    return value;
}

bool rmSndSave(uint32_t value)
{
    return saveCounter("rm_snd", value);
}

void initTimePersistence(void)
{
    if (!RaspberryPiStorage::removeFile("last_time"))
        Serial.printf("[TIME];clear;remove_failed\n");
}

void saveTimePersistence(void)
{
    const unsigned long currentTime = getUnixClock();
    if (currentTime < 1000000000UL)
        return;
    if (!saveCounter("last_time", static_cast<uint32_t>(currentTime)))
        Serial.printf("[TIME];save;failed\n");
}

void loadTimePersistence(void)
{
    uint32_t savedTime = 0;
    parseCounter("last_time", savedTime);

    if (savedTime <= 1000000000UL)
        return;

    MyClock.SetClock(static_cast<time_t>(savedTime) +
                     static_cast<time_t>(meshcom_settings.node_utcoff * 3600.0), false);
    meshcom_settings.node_date_year = MyClock.Year();
    meshcom_settings.node_date_month = MyClock.Month();
    meshcom_settings.node_date_day = MyClock.Day();
    meshcom_settings.node_date_hour = MyClock.Hour();
    meshcom_settings.node_date_minute = MyClock.Minute();
    meshcom_settings.node_date_second = MyClock.Second();
    Serial.printf("[TIME];load;timestamp=%u\n", savedTime);
}
