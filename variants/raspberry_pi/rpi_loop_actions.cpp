#include <Arduino.h>
#include <configuration.h>
#include <lora_functions.h>
#include <loop_functions.h>
#include <loop_functions_extern.h>
#include <loop_scheduler.h>
#include <txring_functions.h>

#define CET_AGE_WITH_CLOCK
#include <cet_age.h>

bool loopEnabled_retransmit(void)
{
    return bRadio;
}

void loopAction_retransmit(void)
{
    updateRetransmissionStatus();
    txRingAgeBackground(millis(), cetDeviceClock());
}

bool loopEnabled_heapMon(void)
{
    return false;
}

void loopAction_heapMon(void)
{
}

void loopAction_battCheck(void)
{
}
