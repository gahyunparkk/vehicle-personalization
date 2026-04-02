#ifndef BASE_DRIVER_DS18B20_LEGACY_H_
#define BASE_DRIVER_DS18B20_LEGACY_H_

/*********************************************************************************************************************/
/*-----------------------------------------------------Includes------------------------------------------------------*/
/*********************************************************************************************************************/
#include "Base_Driver_Ds18b20_Config.h"
#include "Base_Com_OneWire.h"
#include "Platform_Types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*********************************************************************************************************************/
/*------------------------------------------------Function Prototypes------------------------------------------------*/
/*********************************************************************************************************************/
void Base_Driver_Ds18b20_DelayUs(uint16 time_us);
void Base_Driver_Ds18b20_DelayMs(uint16 time_ms);
uint32 Base_Driver_Ds18b20_GetTickMs(void);

/*********************************************************************************************************************/
/*------------------------------------------------Type Definitions--------------------------------------------------*/
/*********************************************************************************************************************/
typedef struct
{
    uint8 address[8];
    float temperature;
    uint8 dataIsValid;
} Base_Driver_Ds18b20_Sensor_t;

typedef enum
{
    Base_Driver_Ds18b20_Resolution_9bits  = 9,
    Base_Driver_Ds18b20_Resolution_10bits = 10,
    Base_Driver_Ds18b20_Resolution_11bits = 11,
    Base_Driver_Ds18b20_Resolution_12bits = 12
} Base_Driver_Ds18b20_Resolution_t;

/*********************************************************************************************************************/
/*-------------------------------------------------Global variables--------------------------------------------------*/
/*********************************************************************************************************************/
extern Base_Driver_Ds18b20_Sensor_t g_base_driver_ds18b20_sensor[BASE_DRIVER_DS18B20_MAX_SENSORS];
extern Base_Com_OneWire_t           g_base_com_onewire; /* TODO: remove after context-based integration */

/*********************************************************************************************************************/
/*------------------------------------------------------Macros-------------------------------------------------------*/
/*********************************************************************************************************************/
/* Every 1-Wire chip has a different ROM code, but the same family shares the first ROM byte.
 * For DS18B20, the family code is 0x28.
 */
#define BASE_DRIVER_DS18B20_FAMILY_CODE           0x28U
#define BASE_DRIVER_DS18B20_CMD_ALARMSEARCH       0xECU
#define BASE_DRIVER_DS18B20_CMD_CONVERTTEMP       0x44U

#define BASE_DRIVER_DS18B20_DECIMAL_STEPS_12BIT   0.0625F
#define BASE_DRIVER_DS18B20_DECIMAL_STEPS_11BIT   0.125F
#define BASE_DRIVER_DS18B20_DECIMAL_STEPS_10BIT   0.25F
#define BASE_DRIVER_DS18B20_DECIMAL_STEPS_9BIT    0.5F

#define BASE_DRIVER_DS18B20_RESOLUTION_R1         6U
#define BASE_DRIVER_DS18B20_RESOLUTION_R0         5U

#ifdef BASE_DRIVER_DS18B20_USE_CRC
#define BASE_DRIVER_DS18B20_DATA_LEN              9U
#else
#define BASE_DRIVER_DS18B20_DATA_LEN              2U
#endif

/*********************************************************************************************************************/
/*------------------------------------------------Function Prototypes------------------------------------------------*/
/*********************************************************************************************************************/
uint8 Base_Driver_Ds18b20_ManualConvert(void);
uint8 Base_Driver_Ds18b20_ManualConvertSingle(void);

uint8 Base_Driver_Ds18b20_Start(Base_Com_OneWire_t *onewire, uint8 *rom);
void  Base_Driver_Ds18b20_StartAll(Base_Com_OneWire_t *onewire);
uint8 Base_Driver_Ds18b20_Read(Base_Com_OneWire_t *onewire, uint8 *rom, float *destination);
uint8 Base_Driver_Ds18b20_GetResolution(Base_Com_OneWire_t *onewire, uint8 *rom);
uint8 Base_Driver_Ds18b20_SetResolution(
    Base_Com_OneWire_t                 *onewire,
    uint8                              *rom,
    Base_Driver_Ds18b20_Resolution_t    resolution);

uint8 Base_Driver_Ds18b20_Is(uint8 *rom);
uint8 Base_Driver_Ds18b20_SetAlarmHighTemperature(Base_Com_OneWire_t *onewire, uint8 *rom, int temp);
uint8 Base_Driver_Ds18b20_SetAlarmLowTemperature(Base_Com_OneWire_t *onewire, uint8 *rom, int temp);
uint8 Base_Driver_Ds18b20_DisableAlarmTemperature(Base_Com_OneWire_t *onewire, uint8 *rom);
uint8 Base_Driver_Ds18b20_AlarmSearch(Base_Com_OneWire_t *onewire);
uint8 Base_Driver_Ds18b20_AllDone(Base_Com_OneWire_t *onewire);

uint8 Base_Driver_Ds18b20_IsTempSensorInit(void);
uint8 Base_Driver_Ds18b20_InitSimple(void);
uint8 Base_Driver_Ds18b20_IsBusyLine(void);
uint8 Base_Driver_Ds18b20_IsConverting(void);
void  Base_Driver_Ds18b20_StartConverting(void);
void  Base_Driver_Ds18b20_CheckConverting(void);
float Base_Driver_Ds18b20_ReadTemperValue(void);

#ifdef __cplusplus
}
#endif

#endif /* BASE_DRIVER_DS18B20_LEGACY_H_ */
