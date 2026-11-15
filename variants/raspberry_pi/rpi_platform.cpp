#include <Arduino.h>
#include <ble_session.h>
#include <configuration.h>
#include <io_functions.h>
#include <loop_functions_extern.h>

BleStats g_bleStats = {};
bool g_ble_uart_is_connected = false;
bool bMCP23017 = false;
uint16_t ADCraw = 0;
float ADCexp1 = 0.0f;
float ADCexp2 = 0.0f;

void setupMCP23017()
{
    bMCP23017 = false;
}

bool loopMCP23017()
{
    return false;
}

String scanI2C()
{
    return "I2C scan unavailable on Raspberry Pi native";
}

void webAuthReset()
{
}

