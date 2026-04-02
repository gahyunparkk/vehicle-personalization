#include "Base_Driver_Ds18b20_Legacy.h"
#include "Base_Driver_Ds18b20_Config.h"
#include "Base_Com_OneWire.h"
#include "Platform_Types.h"
#include "Bsp.h"
#include "UART_Config.h"

#define false 0U
#define true  1U

/*********************************************************************************************************************/
/*-------------------------------------------------Global variables--------------------------------------------------*/
/*********************************************************************************************************************/
Base_Driver_Ds18b20_Sensor_t g_base_driver_ds18b20_sensor[BASE_DRIVER_DS18B20_MAX_SENSORS];

Base_Com_OneWire_t g_base_com_onewire;
uint8              g_base_driver_ds18b20_onewire_devices;
uint8              g_base_driver_ds18b20_sensor_count = 0U;
uint8              g_base_driver_ds18b20_start_convert = 0U;
uint16             g_base_driver_ds18b20_timeout      = 0U;

static uint8 m_init          = 0U;
static uint8 m_busy          = 0U;
static uint8 m_is_converting = 0U;

/*********************************************************************************************************************/
/*------------------------------------------------Function Definitions-----------------------------------------------*/
/*********************************************************************************************************************/
uint32 Base_Driver_Ds18b20_GetTickMs(void)
{
    uint64 ticks = IfxStm_get(BSP_DEFAULT_TIMER);
    uint64 freq  = (uint64)IfxStm_getFrequency(BSP_DEFAULT_TIMER);

    return (uint32)((ticks * 1000ULL) / freq);
}

void Base_Driver_Ds18b20_DelayUs(uint16 time_us)
{
    waitTime(IfxStm_getTicksFromMicroseconds(BSP_DEFAULT_TIMER, time_us));
}

void Base_Driver_Ds18b20_DelayMs(uint16 time_ms)
{
    waitTime(IfxStm_getTicksFromMilliseconds(BSP_DEFAULT_TIMER, time_ms));
}

uint8 Base_Driver_Ds18b20_IsTempSensorInit(void)
{
    return m_init;
}

uint8 Base_Driver_Ds18b20_IsBusyLine(void)
{
    return m_busy;
}

uint8 Base_Driver_Ds18b20_IsConverting(void)
{
    return m_is_converting;
}

uint8 Base_Driver_Ds18b20_InitSimple(void)
{
    m_init = 0U;

    Base_Com_OneWire_Init(&g_base_com_onewire, BASE_DRIVER_DS18B20_GPIO, BASE_DRIVER_DS18B20_PIN);
    g_base_com_onewire.romNo[0] = 0x28U;
    g_base_com_onewire.romNo[1] = 0xDDU;
    g_base_com_onewire.romNo[2] = 0xADU;
    g_base_com_onewire.romNo[3] = 0x87U;
    g_base_com_onewire.romNo[4] = 0x00U;
    g_base_com_onewire.romNo[5] = 0xE3U;
    g_base_com_onewire.romNo[6] = 0x02U;
    g_base_com_onewire.romNo[7] = 0x8CU;

    Base_Com_OneWire_GetFullRom(&g_base_com_onewire, g_base_driver_ds18b20_sensor[0].address);

    Base_Driver_Ds18b20_DelayMs(50U);
    Base_Driver_Ds18b20_SetResolution(
        &g_base_com_onewire,
        g_base_driver_ds18b20_sensor[0].address,
        Base_Driver_Ds18b20_Resolution_12bits);
    Base_Driver_Ds18b20_DelayMs(50U);
    Base_Driver_Ds18b20_DisableAlarmTemperature(
        &g_base_com_onewire,
        g_base_driver_ds18b20_sensor[0].address);

    m_init = 1U;
    g_base_driver_ds18b20_sensor_count++;

    return true;
}

void Base_Driver_Ds18b20_StartConverting(void)
{
    m_busy = 1U;
    Base_Driver_Ds18b20_StartAll(&g_base_com_onewire);
    m_is_converting = 1U;
    m_busy = 0U;
}

void Base_Driver_Ds18b20_CheckConverting(void)
{
    m_busy = 1U;
    m_is_converting = (uint8)!Base_Driver_Ds18b20_AllDone(&g_base_com_onewire);
    m_busy = 0U;
}

float Base_Driver_Ds18b20_ReadTemperValue(void)
{
    Base_Driver_Ds18b20_DelayMs(100U);

    m_busy = 1U;
    g_base_driver_ds18b20_sensor[0].dataIsValid =
        Base_Driver_Ds18b20_Read(
            &g_base_com_onewire,
            g_base_driver_ds18b20_sensor[0].address,
            &g_base_driver_ds18b20_sensor[0].temperature);
    m_busy = 0U;

    return g_base_driver_ds18b20_sensor[0].temperature;
}

uint8 Base_Driver_Ds18b20_ManualConvertSingle(void)
{
    uint16 timeout = BASE_DRIVER_DS18B20_CONVERT_TIMEOUT_MS / 10U;

    Base_Driver_Ds18b20_Start(&g_base_com_onewire, g_base_driver_ds18b20_sensor[0].address);
    Base_Driver_Ds18b20_DelayMs(100U);

    while (Base_Driver_Ds18b20_AllDone(&g_base_com_onewire) == 0U)
    {
        if (timeout == 0U)
        {
            g_base_driver_ds18b20_sensor[0].dataIsValid = false;
            return false;
        }

        timeout--;
        Base_Driver_Ds18b20_DelayMs(10U);
    }

    g_base_driver_ds18b20_sensor[0].dataIsValid =
        Base_Driver_Ds18b20_Read(
            &g_base_com_onewire,
            g_base_driver_ds18b20_sensor[0].address,
            &g_base_driver_ds18b20_sensor[0].temperature);

    return g_base_driver_ds18b20_sensor[0].dataIsValid;
}

uint8 Base_Driver_Ds18b20_Start(Base_Com_OneWire_t *onewire, uint8 *rom)
{
    if (Base_Driver_Ds18b20_Is(rom) == 0U)
    {
        return false;
    }

    Base_Com_OneWire_Reset(onewire);
    Base_Com_OneWire_SelectWithPointer(onewire, rom);
    Base_Com_OneWire_WriteByte(onewire, BASE_DRIVER_DS18B20_CMD_CONVERTTEMP);

    return true;
}

void Base_Driver_Ds18b20_StartAll(Base_Com_OneWire_t *onewire)
{
    Base_Com_OneWire_Reset(onewire);
    Base_Com_OneWire_WriteByte(onewire, BASE_COM_ONEWIRE_CMD_SKIPROM);
    Base_Com_OneWire_WriteByte(onewire, BASE_DRIVER_DS18B20_CMD_CONVERTTEMP);
}

uint8 Base_Driver_Ds18b20_Read(Base_Com_OneWire_t *onewire, uint8 *rom, float *destination)
{
    uint16 temperature;
    uint8  resolution;
    uint8  digit;
    uint8  minus = 0U;
    float  decimal;
    uint8  i = 0U;
    uint8  data[9];
    uint8  crc;

    if (Base_Driver_Ds18b20_Is(rom) == 0U)
    {
        return false;
    }

    if (Base_Com_OneWire_ReadBit(onewire) == 0U)
    {
        return false;
    }

    Base_Com_OneWire_Reset(onewire);
    Base_Com_OneWire_SelectWithPointer(onewire, rom);
    Base_Com_OneWire_WriteByte(onewire, BASE_COM_ONEWIRE_CMD_RSCRATCHPAD);

    for (i = 0U; i < 9U; i++)
    {
        data[i] = Base_Com_OneWire_ReadByte(onewire);
    }

    crc = Base_Com_OneWire_Crc8(data, 8U);

    if (crc != data[8])
    {
        return false;
    }

    temperature = (uint16)(data[0] | (data[1] << 8U));

    Base_Com_OneWire_Reset(onewire);

    if ((temperature & 0x8000U) != 0U)
    {
        temperature = (uint16)(~temperature + 1U);
        minus = 1U;
    }

    resolution = (uint8)(((data[4] & 0x60U) >> 5U) + 9U);

    digit  = (uint8)(temperature >> 4U);
    digit |= (uint8)(((temperature >> 8U) & 0x07U) << 4U);

    switch (resolution)
    {
        case 9U:
            decimal = (float)((temperature >> 3U) & 0x01U);
            decimal *= (float)BASE_DRIVER_DS18B20_DECIMAL_STEPS_9BIT;
            break;

        case 10U:
            decimal = (float)((temperature >> 2U) & 0x03U);
            decimal *= (float)BASE_DRIVER_DS18B20_DECIMAL_STEPS_10BIT;
            break;

        case 11U:
            decimal = (float)((temperature >> 1U) & 0x07U);
            decimal *= (float)BASE_DRIVER_DS18B20_DECIMAL_STEPS_11BIT;
            break;

        case 12U:
            decimal = (float)(temperature & 0x0FU);
            decimal *= (float)BASE_DRIVER_DS18B20_DECIMAL_STEPS_12BIT;
            break;

        default:
            decimal = 0xFFU;
            digit   = 0U;
            break;
    }

    decimal = (float)digit + decimal;

    if (minus != 0U)
    {
        decimal = 0.0F - decimal;
    }

    *destination = decimal;

    return true;
}

uint8 Base_Driver_Ds18b20_GetResolution(Base_Com_OneWire_t *onewire, uint8 *rom)
{
    uint8 conf;

    if (Base_Driver_Ds18b20_Is(rom) == 0U)
    {
        return false;
    }

    Base_Com_OneWire_Reset(onewire);
    Base_Com_OneWire_SelectWithPointer(onewire, rom);
    Base_Com_OneWire_WriteByte(onewire, BASE_COM_ONEWIRE_CMD_RSCRATCHPAD);

    Base_Com_OneWire_ReadByte(onewire);
    Base_Com_OneWire_ReadByte(onewire);
    Base_Com_OneWire_ReadByte(onewire);
    Base_Com_OneWire_ReadByte(onewire);

    conf = Base_Com_OneWire_ReadByte(onewire);

    return (uint8)(((conf & 0x60U) >> 5U) + 9U);
}

uint8 Base_Driver_Ds18b20_SetResolution(
    Base_Com_OneWire_t                  *onewire,
    uint8                               *rom,
    Base_Driver_Ds18b20_Resolution_t     resolution)
{
    uint8 th;
    uint8 tl;
    uint8 conf;

    if (Base_Driver_Ds18b20_Is(rom) == 0U)
    {
        return false;
    }

    Base_Com_OneWire_Reset(onewire);
    Base_Com_OneWire_SelectWithPointer(onewire, rom);
    Base_Com_OneWire_WriteByte(onewire, BASE_COM_ONEWIRE_CMD_RSCRATCHPAD);

    Base_Com_OneWire_ReadByte(onewire);
    Base_Com_OneWire_ReadByte(onewire);

    th   = Base_Com_OneWire_ReadByte(onewire);
    tl   = Base_Com_OneWire_ReadByte(onewire);
    conf = Base_Com_OneWire_ReadByte(onewire);

    if (resolution == Base_Driver_Ds18b20_Resolution_9bits)
    {
        conf &= (uint8)~(1U << BASE_DRIVER_DS18B20_RESOLUTION_R1);
        conf &= (uint8)~(1U << BASE_DRIVER_DS18B20_RESOLUTION_R0);
    }
    else if (resolution == Base_Driver_Ds18b20_Resolution_10bits)
    {
        conf &= (uint8)~(1U << BASE_DRIVER_DS18B20_RESOLUTION_R1);
        conf |= (uint8)(1U << BASE_DRIVER_DS18B20_RESOLUTION_R0);
    }
    else if (resolution == Base_Driver_Ds18b20_Resolution_11bits)
    {
        conf |= (uint8)(1U << BASE_DRIVER_DS18B20_RESOLUTION_R1);
        conf &= (uint8)~(1U << BASE_DRIVER_DS18B20_RESOLUTION_R0);
    }
    else if (resolution == Base_Driver_Ds18b20_Resolution_12bits)
    {
        conf |= (uint8)(1U << BASE_DRIVER_DS18B20_RESOLUTION_R1);
        conf |= (uint8)(1U << BASE_DRIVER_DS18B20_RESOLUTION_R0);
    }
    else
    {
        /* do nothing */
    }

    Base_Com_OneWire_Reset(onewire);
    Base_Com_OneWire_SelectWithPointer(onewire, rom);
    Base_Com_OneWire_WriteByte(onewire, BASE_COM_ONEWIRE_CMD_WSCRATCHPAD);

    Base_Com_OneWire_WriteByte(onewire, th);
    Base_Com_OneWire_WriteByte(onewire, tl);
    Base_Com_OneWire_WriteByte(onewire, conf);

    Base_Com_OneWire_Reset(onewire);
    Base_Com_OneWire_SelectWithPointer(onewire, rom);
    Base_Com_OneWire_WriteByte(onewire, BASE_COM_ONEWIRE_CMD_CPYSCRATCHPAD);

    return true;
}

uint8 Base_Driver_Ds18b20_Is(uint8 *rom)
{
    if (*rom == BASE_DRIVER_DS18B20_FAMILY_CODE)
    {
        return true;
    }

    return false;
}

uint8 Base_Driver_Ds18b20_SetAlarmLowTemperature(Base_Com_OneWire_t *onewire, uint8 *rom, int temp)
{
    uint8 tl;
    uint8 th;
    uint8 conf;

    if (Base_Driver_Ds18b20_Is(rom) == 0U)
    {
        return false;
    }

    if (temp > 125)
    {
        temp = 125;
    }

    if (temp < -55)
    {
        temp = -55;
    }

    Base_Com_OneWire_Reset(onewire);
    Base_Com_OneWire_SelectWithPointer(onewire, rom);
    Base_Com_OneWire_WriteByte(onewire, BASE_COM_ONEWIRE_CMD_RSCRATCHPAD);

    Base_Com_OneWire_ReadByte(onewire);
    Base_Com_OneWire_ReadByte(onewire);

    th   = Base_Com_OneWire_ReadByte(onewire);
    tl   = Base_Com_OneWire_ReadByte(onewire);
    conf = Base_Com_OneWire_ReadByte(onewire);

    tl = (uint8)temp;

    Base_Com_OneWire_Reset(onewire);
    Base_Com_OneWire_SelectWithPointer(onewire, rom);
    Base_Com_OneWire_WriteByte(onewire, BASE_COM_ONEWIRE_CMD_WSCRATCHPAD);

    Base_Com_OneWire_WriteByte(onewire, th);
    Base_Com_OneWire_WriteByte(onewire, tl);
    Base_Com_OneWire_WriteByte(onewire, conf);

    Base_Com_OneWire_Reset(onewire);
    Base_Com_OneWire_SelectWithPointer(onewire, rom);
    Base_Com_OneWire_WriteByte(onewire, BASE_COM_ONEWIRE_CMD_CPYSCRATCHPAD);

    return true;
}

uint8 Base_Driver_Ds18b20_SetAlarmHighTemperature(Base_Com_OneWire_t *onewire, uint8 *rom, int temp)
{
    uint8 tl;
    uint8 th;
    uint8 conf;

    if (Base_Driver_Ds18b20_Is(rom) == 0U)
    {
        return false;
    }

    if (temp > 125)
    {
        temp = 125;
    }

    if (temp < -55)
    {
        temp = -55;
    }

    Base_Com_OneWire_Reset(onewire);
    Base_Com_OneWire_SelectWithPointer(onewire, rom);
    Base_Com_OneWire_WriteByte(onewire, BASE_COM_ONEWIRE_CMD_RSCRATCHPAD);

    Base_Com_OneWire_ReadByte(onewire);
    Base_Com_OneWire_ReadByte(onewire);

    th   = Base_Com_OneWire_ReadByte(onewire);
    tl   = Base_Com_OneWire_ReadByte(onewire);
    conf = Base_Com_OneWire_ReadByte(onewire);

    th = (uint8)temp;

    Base_Com_OneWire_Reset(onewire);
    Base_Com_OneWire_SelectWithPointer(onewire, rom);
    Base_Com_OneWire_WriteByte(onewire, BASE_COM_ONEWIRE_CMD_WSCRATCHPAD);

    Base_Com_OneWire_WriteByte(onewire, th);
    Base_Com_OneWire_WriteByte(onewire, tl);
    Base_Com_OneWire_WriteByte(onewire, conf);

    Base_Com_OneWire_Reset(onewire);
    Base_Com_OneWire_SelectWithPointer(onewire, rom);
    Base_Com_OneWire_WriteByte(onewire, BASE_COM_ONEWIRE_CMD_CPYSCRATCHPAD);

    return true;
}

uint8 Base_Driver_Ds18b20_DisableAlarmTemperature(Base_Com_OneWire_t *onewire, uint8 *rom)
{
    uint8 tl;
    uint8 th;
    uint8 conf;

    if (Base_Driver_Ds18b20_Is(rom) == 0U)
    {
        return false;
    }

    Base_Com_OneWire_Reset(onewire);
    Base_Com_OneWire_SelectWithPointer(onewire, rom);
    Base_Com_OneWire_WriteByte(onewire, BASE_COM_ONEWIRE_CMD_RSCRATCHPAD);

    Base_Com_OneWire_ReadByte(onewire);
    Base_Com_OneWire_ReadByte(onewire);

    th   = Base_Com_OneWire_ReadByte(onewire);
    tl   = Base_Com_OneWire_ReadByte(onewire);
    conf = Base_Com_OneWire_ReadByte(onewire);

    th = 125U;
    tl = (uint8)-55;

    Base_Com_OneWire_Reset(onewire);
    Base_Com_OneWire_SelectWithPointer(onewire, rom);
    Base_Com_OneWire_WriteByte(onewire, BASE_COM_ONEWIRE_CMD_WSCRATCHPAD);

    Base_Com_OneWire_WriteByte(onewire, th);
    Base_Com_OneWire_WriteByte(onewire, tl);
    Base_Com_OneWire_WriteByte(onewire, conf);

    Base_Com_OneWire_Reset(onewire);
    Base_Com_OneWire_SelectWithPointer(onewire, rom);
    Base_Com_OneWire_WriteByte(onewire, BASE_COM_ONEWIRE_CMD_CPYSCRATCHPAD);

    return true;
}

uint8 Base_Driver_Ds18b20_AlarmSearch(Base_Com_OneWire_t *onewire)
{
    return Base_Com_OneWire_Search(onewire, BASE_DRIVER_DS18B20_CMD_ALARMSEARCH);
}

uint8 Base_Driver_Ds18b20_AllDone(Base_Com_OneWire_t *onewire)
{
    return Base_Com_OneWire_ReadBit(onewire);
}
