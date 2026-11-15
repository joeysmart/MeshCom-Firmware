#pragma once

#include <Arduino.h>
#include "FreeRTOS.h"

inline void vTaskDelay(TickType_t ticks)
{
    delay(ticks);
}
