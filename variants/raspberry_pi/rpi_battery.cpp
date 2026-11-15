#include <Arduino.h>
#include <batt_functions.h>
#include <configuration.h>

batt_probe_t battProbeState = BATT_PROBE_NONE;
BattChargeInfo global_batt_charge = {BCS_UNKNOWN, false, 0};
float global_batt = 0.0f;
int global_proz = 0;
unsigned long BattTimeWait = 0;

uint32_t battSampleCount(void)
{
    return 0;
}

bool battHardwarePresent(void)
{
    return false;
}

void init_batt(void)
{
}

float read_batt(void)
{
    return 0.0f;
}

uint8_t mv_to_percent(float)
{
    return 0;
}

void setMaxBatt(float)
{
}

void check_efuse(void)
{
}

void VextON(void)
{
}

void VextOFF(void)
{
}

void ADC_BATT_ON(void)
{
}

void ADC_BATT_OFF(void)
{
}
