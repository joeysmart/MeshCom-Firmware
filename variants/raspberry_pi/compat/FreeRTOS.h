#pragma once

#include <cstdint>
#include <mutex>

namespace RaspberryPiFreeRTOS
{
inline std::recursive_mutex criticalSection;
}

using BaseType_t = int;
using UBaseType_t = unsigned int;
using TickType_t = uint32_t;
using portMUX_TYPE = std::recursive_mutex;

#define pdTRUE 1
#define pdFALSE 0
#define portMAX_DELAY UINT32_MAX
#define pdMS_TO_TICKS(ms) (static_cast<TickType_t>(ms))
#define portMUX_INITIALIZER_UNLOCKED {}
#define taskENTER_CRITICAL() RaspberryPiFreeRTOS::criticalSection.lock()
#define taskEXIT_CRITICAL() RaspberryPiFreeRTOS::criticalSection.unlock()
#define portENTER_CRITICAL(mux) (mux)->lock()
#define portEXIT_CRITICAL(mux) (mux)->unlock()
