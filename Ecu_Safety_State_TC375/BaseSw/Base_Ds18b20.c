#include "Base_Ds18b20.h"
#include "Platform_Types.h"
#include "Bsp.h"
#include "UART_Config.h"
#define false 0
#define true  1

//###################################################################################
Ds18b20Sensor_t ds18b20[_DS18B20_MAX_SENSORS];

OneWire_t OneWire;
uint8     OneWireDevices;
uint8     TempSensorCount=0;
uint8     Ds18b20StartConvert=0;
uint16    Ds18b20Timeout=0;

static uint8 m_init = 0;
static uint8 m_busy = 0;
static uint8 m_isConverting = 0;

//###########################################################################################

uint32 Ds18b20_GetTickMs(void) {
    uint64 ticks = IfxStm_get(BSP_DEFAULT_TIMER);
    uint64 freq  = (uint64)IfxStm_getFrequency(BSP_DEFAULT_TIMER);

    return (uint32)((ticks * 1000ULL) / freq);
}


void Ds18b20_DelayUs(uint16 time_us) {
    waitTime(IfxStm_getTicksFromMicroseconds(BSP_DEFAULT_TIMER, time_us));
}


void Ds18b20_DelayMs(uint16 time_ms) {
    waitTime(IfxStm_getTicksFromMilliseconds(BSP_DEFAULT_TIMER, time_ms));
}


uint8 Ds18b20_IsTemperSensorInit() {
    return m_init;
}


uint8 Ds18b20_IsBusyLine() {
    return m_busy;
}


uint8 Ds18b20_IsConverting() {
    return m_isConverting;
}

//###########################################################################################

uint8 Ds18b20_InitSimple() {
    m_init = 0;
    OneWire_Init(&OneWire, _DS18B20_GPIO ,_DS18B20_PIN);

    OneWire.ROM_NO[0] = 0x28;
    OneWire.ROM_NO[1] = 0xdd;
    OneWire.ROM_NO[2] = 0xad;
    OneWire.ROM_NO[3] = 0x87;
    OneWire.ROM_NO[4] = 0x00;
    OneWire.ROM_NO[5] = 0xe3;
    OneWire.ROM_NO[6] = 0x02;
    OneWire.ROM_NO[7] = 0x8c;
    OneWire_GetFullROM(&OneWire, ds18b20[0].Address);

    Ds18b20_DelayMs(50);
    Ds18b20_SetResolution(&OneWire, ds18b20[0].Address, Ds18b20_Resolution_12bits);
    Ds18b20_DelayMs(50);
    Ds18b20_DisableAlarmTemperature(&OneWire,  ds18b20[0].Address);
    m_init = 1;
    TempSensorCount++;
    return true;
}

//###########################################################################################

void StartConverting() {
    m_busy = 1;
    Ds18b20_StartAll(&OneWire);
    m_isConverting = 1;
    m_busy = 0;
}

void checkConverting() {
    m_busy = 1;
    m_isConverting = !Ds18b20_AllDone(&OneWire); //완료 1,비완료 0
    m_busy = 0;
}

float Ds18b20_ReadTemperValue() {
    Ds18b20_DelayMs(100);
    m_busy = 1;
    ds18b20[0].DataIsValid = Ds18b20_Read(&OneWire, ds18b20[0].Address, &ds18b20[0].Temperature);
    m_busy = 0;
    return ds18b20[0].Temperature;
}


uint8 Ds18b20_ManualConvertSingle(void) {
  uint16 timeout = _DS18B20_CONVERT_TIMEOUT_MS / 10U;

  // request temperature conversion to the only connected one device.
  Ds18b20_Start(&OneWire, ds18b20[0].Address);
  Ds18b20_DelayMs(100);

  // check if conversion job is finished.
  while (!Ds18b20_AllDone(&OneWire)) {
    if (timeout == 0U) {
      ds18b20[0].DataIsValid = false;
      return false;
    }

    timeout--;
    Ds18b20_DelayMs(10);
  }

  // store temperature into ds18b20.
  ds18b20[0].DataIsValid = Ds18b20_Read(&OneWire, ds18b20[0].Address, &ds18b20[0].Temperature);

  return ds18b20[0].DataIsValid;
}

//###########################################################################################

uint8 Ds18b20_Start(OneWire_t* OneWire, uint8 *ROM)
{
    /* Check if device is DS18B20 */
    if (!Ds18b20_Is(ROM)) {
        return false;
    }

    /* Reset line */
    OneWire_Reset(OneWire);
    /* Select ROM number */
    OneWire_SelectWithPointer(OneWire, ROM);

    /* Start temperature conversion */
    OneWire_WriteByte(OneWire, DS18B20_CMD_CONVERTTEMP);

    return true;
}


void Ds18b20_StartAll(OneWire_t* OneWire)
{
    /* Reset pulse */
    OneWire_Reset(OneWire);
    /* Skip rom */
    OneWire_WriteByte(OneWire, ONEWIRE_CMD_SKIPROM);
    /* Start conversion on all connected devices */
    OneWire_WriteByte(OneWire, DS18B20_CMD_CONVERTTEMP);
}


uint8 Ds18b20_Read(OneWire_t* OneWire, uint8 *ROM, float *destination)
{
    uint16 temperature;
    uint8 resolution;
    uint8 digit, minus = 0;
    float decimal;
    uint8 i = 0;
    uint8 data[9];
    uint8 crc;

    /* Check if device is DS18B20 */
    if (!Ds18b20_Is(ROM)) {
        return false;
    }

    /* Check if line is released, if it is, then conversion is complete */
    if (!OneWire_ReadBit(OneWire))
    {
        /* Conversion is not finished yet */
        return false;
    }

    /* Reset line */
    OneWire_Reset(OneWire);
    /* Select ROM number */
    OneWire_SelectWithPointer(OneWire, ROM);
    /* Read scratchpad command by onewire protocol */
    OneWire_WriteByte(OneWire, ONEWIRE_CMD_RSCRATCHPAD);

    /* Get data */
    for (i = 0; i < 9; i++)
    {
        /* Read byte by byte */
        data[i] = OneWire_ReadByte(OneWire);
    }

    /* Calculate CRC */
    crc = OneWire_CRC8(data, 8);

    /* Check if CRC is ok */
    if (crc != data[8])
        /* CRC invalid */
        return false;


    /* First two bytes of scratchpad are temperature values */
    temperature = data[0] | (data[1] << 8);

    /* Reset line */
    OneWire_Reset(OneWire);

    /* Check if temperature is negative */
    if (temperature & 0x8000)
    {
        /* Two's complement, temperature is negative */
        temperature = ~temperature + 1;
        minus = 1;
    }


    /* Get sensor resolution */
    resolution = ((data[4] & 0x60) >> 5) + 9;


    /* Store temperature integer digits and decimal digits */
    digit = temperature >> 4;
    digit |= ((temperature >> 8) & 0x7) << 4;

    /* Store decimal digits */
    switch (resolution)
    {
        case 9:
            decimal = (temperature >> 3) & 0x01;
            decimal *= (float)DS18B20_DECIMAL_STEPS_9BIT;
        break;
        case 10:
            decimal = (temperature >> 2) & 0x03;
            decimal *= (float)DS18B20_DECIMAL_STEPS_10BIT;
         break;
        case 11:
            decimal = (temperature >> 1) & 0x07;
            decimal *= (float)DS18B20_DECIMAL_STEPS_11BIT;
        break;
        case 12:
            decimal = temperature & 0x0F;
            decimal *= (float)DS18B20_DECIMAL_STEPS_12BIT;
         break;
        default:
            decimal = 0xFF;
            digit = 0;
    }

    /* Check for negative part */
    decimal = digit + decimal;
    if (minus)
        decimal = 0 - decimal;


    /* Set to pointer */
    *destination = decimal;

    /* return true, temperature valid */
    return true;
}

//###########################################################################################

uint8 Ds18b20_GetResolution(OneWire_t* OneWire, uint8 *ROM)
{
    uint8 conf;

    if (!Ds18b20_Is(ROM))
        return false;

    /* Reset line */
    OneWire_Reset(OneWire);
    /* Select ROM number */
    OneWire_SelectWithPointer(OneWire, ROM);
    /* Read scratchpad command by onewire protocol */
    OneWire_WriteByte(OneWire, ONEWIRE_CMD_RSCRATCHPAD);

    /* Ignore first 4 bytes */
    OneWire_ReadByte(OneWire);
    OneWire_ReadByte(OneWire);
    OneWire_ReadByte(OneWire);
    OneWire_ReadByte(OneWire);

    /* 5th byte of scratchpad is configuration register */
    conf = OneWire_ReadByte(OneWire);

    /* Return 9 - 12 value according to number of bits */
    return ((conf & 0x60) >> 5) + 9;
}

uint8 Ds18b20_SetResolution(OneWire_t* OneWire, uint8 *ROM, DS18B20_Resolution_t resolution)
{
    uint8 th, tl, conf;
    if (!Ds18b20_Is(ROM))
        return false;

    /* 1. Reset line */
    OneWire_Reset(OneWire);
    /* 2. Select ROM number */
    OneWire_SelectWithPointer(OneWire, ROM);
    /* 3. Read scratchpad command by onewire protocol */
    OneWire_WriteByte(OneWire, ONEWIRE_CMD_RSCRATCHPAD);

    /* Ignore first 2 bytes */
    OneWire_ReadByte(OneWire);
    OneWire_ReadByte(OneWire);

    th = OneWire_ReadByte(OneWire);
    tl = OneWire_ReadByte(OneWire);
    conf = OneWire_ReadByte(OneWire);

    if (resolution == Ds18b20_Resolution_9bits)
    {
        conf &= ~(1 << DS18B20_RESOLUTION_R1);
        conf &= ~(1 << DS18B20_RESOLUTION_R0);
    }
    else if (resolution == Ds18b20_Resolution_10bits)
    {
        conf &= ~(1 << DS18B20_RESOLUTION_R1);
        conf |= 1 << DS18B20_RESOLUTION_R0;
    }
    else if (resolution == Ds18b20_Resolution_11bits)
    {
        conf |= 1 << DS18B20_RESOLUTION_R1;
        conf &= ~(1 << DS18B20_RESOLUTION_R0);
    }
    else if (resolution == Ds18b20_Resolution_12bits)
    {
        conf |= 1 << DS18B20_RESOLUTION_R1;
        conf |= 1 << DS18B20_RESOLUTION_R0;
    }

    /* Reset line */
    OneWire_Reset(OneWire);
    /* Select ROM number */
    OneWire_SelectWithPointer(OneWire, ROM);
    /* Write scratchpad command by onewire protocol, only th, tl and conf register can be written */
    OneWire_WriteByte(OneWire, ONEWIRE_CMD_WSCRATCHPAD);

    /* Write bytes */
    OneWire_WriteByte(OneWire, th);
    OneWire_WriteByte(OneWire, tl);
    OneWire_WriteByte(OneWire, conf);

    /* Reset line */
    OneWire_Reset(OneWire);
    /* Select ROM number */
    OneWire_SelectWithPointer(OneWire, ROM);
    /* Copy scratchpad to EEPROM of DS18B20 */
    OneWire_WriteByte(OneWire, ONEWIRE_CMD_CPYSCRATCHPAD);

    return true;
}


//###########################################################################################


uint8 Ds18b20_Is(uint8 *ROM)
{
    /* Checks if first byte is equal to DS18B20's family code */
    if (*ROM == DS18B20_FAMILY_CODE)
        return true;

    return false;
}


uint8 Ds18b20_SetAlarmLowTemperature(OneWire_t* OneWire, uint8 *ROM, int temp)
{
    uint8 tl, th, conf;
    if (!Ds18b20_Is(ROM))
        return false;

    if (temp > 125)
        temp = 125;

    if (temp < -55)
        temp = -55;

    /* Reset line */
    OneWire_Reset(OneWire);
    /* Select ROM number */
    OneWire_SelectWithPointer(OneWire, ROM);
    /* Read scratchpad command by onewire protocol */
    OneWire_WriteByte(OneWire, ONEWIRE_CMD_RSCRATCHPAD);

    /* Ignore first 2 bytes */
    OneWire_ReadByte(OneWire);
    OneWire_ReadByte(OneWire);

    th = OneWire_ReadByte(OneWire);
    tl = OneWire_ReadByte(OneWire);
    conf = OneWire_ReadByte(OneWire);

    tl = (uint8)temp;

    /* Reset line */
    OneWire_Reset(OneWire);
    /* Select ROM number */
    OneWire_SelectWithPointer(OneWire, ROM);
    /* Write scratchpad command by onewire protocol, only th, tl and conf register can be written */
    OneWire_WriteByte(OneWire, ONEWIRE_CMD_WSCRATCHPAD);

    /* Write bytes */
    OneWire_WriteByte(OneWire, th);
    OneWire_WriteByte(OneWire, tl);
    OneWire_WriteByte(OneWire, conf);

    /* Reset line */
    OneWire_Reset(OneWire);
    /* Select ROM number */
    OneWire_SelectWithPointer(OneWire, ROM);
    /* Copy scratchpad to EEPROM of DS18B20 */
    OneWire_WriteByte(OneWire, ONEWIRE_CMD_CPYSCRATCHPAD);

    return true;
}

uint8 Ds18b20_SetAlarmHighTemperature(OneWire_t* OneWire, uint8 *ROM, int temp)
{
    uint8 tl, th, conf;
    if (!Ds18b20_Is(ROM))
        return false;

    if (temp > 125)
        temp = 125;

    if (temp < -55)
        temp = -55;

    /* Reset line */
    OneWire_Reset(OneWire);
    /* Select ROM number */
    OneWire_SelectWithPointer(OneWire, ROM);
    /* Read scratchpad command by onewire protocol */
    OneWire_WriteByte(OneWire, ONEWIRE_CMD_RSCRATCHPAD);

    /* Ignore first 2 bytes */
    OneWire_ReadByte(OneWire);
    OneWire_ReadByte(OneWire);

    th = OneWire_ReadByte(OneWire);
    tl = OneWire_ReadByte(OneWire);
    conf = OneWire_ReadByte(OneWire);

    th = (uint8)temp;

    /* Reset line */
    OneWire_Reset(OneWire);
    /* Select ROM number */
    OneWire_SelectWithPointer(OneWire, ROM);
    /* Write scratchpad command by onewire protocol, only th, tl and conf register can be written */
    OneWire_WriteByte(OneWire, ONEWIRE_CMD_WSCRATCHPAD);

    /* Write bytes */
    OneWire_WriteByte(OneWire, th);
    OneWire_WriteByte(OneWire, tl);
    OneWire_WriteByte(OneWire, conf);

    /* Reset line */
    OneWire_Reset(OneWire);
    /* Select ROM number */
    OneWire_SelectWithPointer(OneWire, ROM);
    /* Copy scratchpad to EEPROM of DS18B20 */
    OneWire_WriteByte(OneWire, ONEWIRE_CMD_CPYSCRATCHPAD);

    return true;
}

uint8 Ds18b20_DisableAlarmTemperature(OneWire_t* OneWire, uint8 *ROM)
{
    uint8 tl, th, conf;
    if (!Ds18b20_Is(ROM))
        return false;

    /* Reset line */
    OneWire_Reset(OneWire);
    /* Select ROM number */
    OneWire_SelectWithPointer(OneWire, ROM);
    /* Read scratchpad command by onewire protocol */
    OneWire_WriteByte(OneWire, ONEWIRE_CMD_RSCRATCHPAD);

    /* Ignore first 2 bytes */
    OneWire_ReadByte(OneWire);
    OneWire_ReadByte(OneWire);

    th = OneWire_ReadByte(OneWire);
    tl = OneWire_ReadByte(OneWire);
    conf = OneWire_ReadByte(OneWire);

    th = 125;
    tl = (uint8)-55;

    /* Reset line */
    OneWire_Reset(OneWire);
    /* Select ROM number */
    OneWire_SelectWithPointer(OneWire, ROM);
    /* Write scratchpad command by onewire protocol, only th, tl and conf register can be written */
    OneWire_WriteByte(OneWire, ONEWIRE_CMD_WSCRATCHPAD);

    /* Write bytes */
    OneWire_WriteByte(OneWire, th);
    OneWire_WriteByte(OneWire, tl);
    OneWire_WriteByte(OneWire, conf);

    /* Reset line */
    OneWire_Reset(OneWire);
    /* Select ROM number */
    OneWire_SelectWithPointer(OneWire, ROM);
    /* Copy scratchpad to EEPROM of DS18B20 */
    OneWire_WriteByte(OneWire, ONEWIRE_CMD_CPYSCRATCHPAD);

    return true;
}

uint8 Ds18b20_AlarmSearch(OneWire_t* OneWire)
{
    /* Start alarm search */
    return OneWire_Search(OneWire, DS18B20_CMD_ALARMSEARCH);
}

//###########################################################################################

uint8 Ds18b20_AllDone(OneWire_t* OneWire)
{
    /* If read bit is low, then device is not finished yet with calculation temperature */
    return OneWire_ReadBit(OneWire);
}

//###########################################################################################

//uint8 Ds18b20_Init(void) {
//    uint8 Ds18b20TryToFind=5;
//    do
//    {
//        OneWire_Init(&OneWire, _DS18B20_GPIO ,_DS18B20_PIN);
//        TempSensorCount = 0;
//        while(Ds18b20_GetTickMs() < 3000)
//            Ds18b20_DelayMs(100);
//        OneWireDevices = OneWire_First(&OneWire); //주소를 찾는 동작.
//        while (OneWireDevices)
//        {
//            Ds18b20_DelayMs(100);
//            TempSensorCount++;
//            OneWire_GetFullROM(&OneWire, ds18b20[TempSensorCount-1].Address);
//            OneWireDevices = OneWire_Next(&OneWire); //다음 장치 주소 찾는 동작.
//        }
//        if(TempSensorCount>0)
//            break;
//        Ds18b20TryToFind--;
//    }while(Ds18b20TryToFind>0);
//    if(Ds18b20TryToFind==0)
//        return false;
//
//    for (uint8 i = 0; i < TempSensorCount; i++)
//    {
//        Ds18b20_DelayMs(50);
//        Ds18b20_SetResolution(&OneWire, ds18b20[i].Address, Ds18b20_Resolution_12bits);
//        Ds18b20_DelayMs(50);
//    Ds18b20_DisableAlarmTemperature(&OneWire, ds18b20[i].Address);
//    }
//    return true;
//}


