#ifndef BASESW_BASE_DS18B20_H_
#define BASESW_BASE_DS18B20_H_

#include "Base_OneWire.h"
#include "Base_Ds18b20_Config.h"

void Ds18b20_Delay(uint16 time_us);

//###################################################################################
typedef struct
{
    uint8       Address[8];
    float       Temperature;
    uint8       DataIsValid;

} Ds18b20Sensor_t;

//###################################################################################

extern Ds18b20Sensor_t  ds18b20[_DS18B20_MAX_SENSORS];
extern Ds18b20Sensor_t  temp_sensor;

extern OneWire_t        OneWire; // 나중에 지울거임 (TODO: DELETE)

//###################################################################################
/* Every onewire chip has different ROM code, but all the same chips has same family code */
/* in case of DS18B20 this is 0x28 and this is first byte of ROM address */
#define DS18B20_FAMILY_CODE             0x28
#define DS18B20_CMD_ALARMSEARCH         0xEC

/* DS18B20 read temperature command */
#define DS18B20_CMD_CONVERTTEMP         0x44    /* Convert temperature */
#define DS18B20_DECIMAL_STEPS_12BIT     0.0625
#define DS18B20_DECIMAL_STEPS_11BIT     0.125
#define DS18B20_DECIMAL_STEPS_10BIT     0.25
#define DS18B20_DECIMAL_STEPS_9BIT      0.5

/* Bits locations for resolution */
#define DS18B20_RESOLUTION_R1           6
#define DS18B20_RESOLUTION_R0           5

/* CRC enabled */
#ifdef DS18B20_USE_CRC
#define DS18B20_DATA_LEN                9
#else
#define DS18B20_DATA_LEN                2
#endif

//###################################################################################
typedef enum {
    Ds18b20_Resolution_9bits  = 9,  /*!< DS18B20 9 bits resolution */
    Ds18b20_Resolution_10bits = 10, /*!< DS18B20 10 bits resolution */
    Ds18b20_Resolution_11bits = 11, /*!< DS18B20 11 bits resolution */
    Ds18b20_Resolution_12bits = 12  /*!< DS18B20 12 bits resolution */
} DS18B20_Resolution_t;

//###################################################################################
uint8     Ds18b20_ManualConvert(void);
uint8     Ds18b20_ManualConvertSingle(void);

//###################################################################################
uint8     Ds18b20_Start(OneWire_t* OneWireStruct, uint8* ROM);
void      Ds18b20_StartAll(OneWire_t* OneWireStruct);
uint8     Ds18b20_Read(OneWire_t* OneWireStruct, uint8* ROM, float* destination);
uint8     Ds18b20_GetResolution(OneWire_t* OneWireStruct, uint8* ROM);
uint8     Ds18b20_SetResolution(OneWire_t* OneWireStruct, uint8* ROM, DS18B20_Resolution_t resolution);
uint8     Ds18b20_Is(uint8* ROM);
uint8     Ds18b20_SetAlarmHighTemperature(OneWire_t* OneWireStruct, uint8* ROM, int temp);
uint8     Ds18b20_SetAlarmLowTemperature(OneWire_t* OneWireStruct, uint8* ROM, int temp);
uint8     Ds18b20_DisableAlarmTemperature(OneWire_t* OneWireStruct, uint8* ROM);
uint8     Ds18b20_AlarmSearch(OneWire_t* OneWireStruct);
uint8     Ds18b20_AllDone(OneWire_t* OneWireStruct);
uint8     Ds18b20_IsTemperSensorInit(void);
uint8     Ds18b20_InitSimple(void);
uint8     Ds18b20_IsBusyLine(void);
uint8     Ds18b20_IsConverting(void);
void      Ds18b20_StartConverting(void);
void      Ds18b20_CheckConverting(void);
float     Ds18b20_ReadTemperValue(void);

void      Ds18b20_DelayMs(uint16 time_ms);


//###################################################################################


#endif /* BASESW_BASE_DS18B20_H_ */
