#ifndef BASE_COM_ONEWIRE_H_
#define BASE_COM_ONEWIRE_H_

/*********************************************************************************************************************/
/*-----------------------------------------------------Includes------------------------------------------------------*/
/*********************************************************************************************************************/
#include "Platform_Types.h"
#include "IfxPort.h"

#ifdef __cplusplus
extern "C" {
#endif

/*********************************************************************************************************************/
/*------------------------------------------------Type Definitions--------------------------------------------------*/
/*********************************************************************************************************************/
typedef struct
{
    Ifx_P *port;
    uint8  pinIndex;
} Base_Com_Gpio_Struct;

typedef struct
{
    Base_Com_Gpio_Struct dq;
    uint8                lastDiscrepancy;         /* Search private */
    uint8                lastFamilyDiscrepancy;   /* Search private */
    uint8                lastDeviceFlag;          /* Search private */
    uint8                romNo[8];                /* 8-byte address of last search device */
} Base_Com_OneWire_t;

/*********************************************************************************************************************/
/*------------------------------------------------------Macros-------------------------------------------------------*/
/*********************************************************************************************************************/
#define BASE_COM_ONEWIRE_CMD_RSCRATCHPAD      0xBEU
#define BASE_COM_ONEWIRE_CMD_WSCRATCHPAD      0x4EU
#define BASE_COM_ONEWIRE_CMD_CPYSCRATCHPAD    0x48U
#define BASE_COM_ONEWIRE_CMD_RECEEPROM        0xB8U
#define BASE_COM_ONEWIRE_CMD_RPWRSUPPLY       0xB4U
#define BASE_COM_ONEWIRE_CMD_SEARCHROM        0xF0U
#define BASE_COM_ONEWIRE_CMD_READROM          0x33U
#define BASE_COM_ONEWIRE_CMD_MATCHROM         0x55U
#define BASE_COM_ONEWIRE_CMD_SKIPROM          0xCCU

/*********************************************************************************************************************/
/*------------------------------------------------Function Prototypes------------------------------------------------*/
/*********************************************************************************************************************/

/* OneWire delay */
void Base_Com_OneWire_Delay(uint16 time_us);

/* Pin settings */
void Base_Com_OneWire_Low(Base_Com_OneWire_t *oneWire);
void Base_Com_OneWire_High(Base_Com_OneWire_t *oneWire);
void Base_Com_OneWire_Input(Base_Com_OneWire_t *oneWire);
void Base_Com_OneWire_Output(Base_Com_OneWire_t *oneWire);

/* OneWire core */
void  Base_Com_OneWire_Init(Base_Com_OneWire_t *oneWire, Ifx_P *port, uint8 pinIndex);
uint8 Base_Com_OneWire_Reset(Base_Com_OneWire_t *oneWire);
uint8 Base_Com_OneWire_ReadByte(Base_Com_OneWire_t *oneWire);
void  Base_Com_OneWire_WriteByte(Base_Com_OneWire_t *oneWire, uint8 byte);
void  Base_Com_OneWire_WriteBit(Base_Com_OneWire_t *oneWire, uint8 bit);
uint8 Base_Com_OneWire_ReadBit(Base_Com_OneWire_t *oneWire);
uint8 Base_Com_OneWire_Search(Base_Com_OneWire_t *oneWire, uint8 command);
void  Base_Com_OneWire_ResetSearch(Base_Com_OneWire_t *oneWire);
uint8 Base_Com_OneWire_First(Base_Com_OneWire_t *oneWire);
uint8 Base_Com_OneWire_Next(Base_Com_OneWire_t *oneWire);
void  Base_Com_OneWire_GetFullRom(Base_Com_OneWire_t *oneWire, uint8 *firstIndex);
void  Base_Com_OneWire_Select(Base_Com_OneWire_t *oneWire, uint8 *addr);
void  Base_Com_OneWire_SelectWithPointer(Base_Com_OneWire_t *oneWire, uint8 *rom);
uint8 Base_Com_OneWire_Crc8(uint8 *addr, uint8 len);
uint8 Base_Com_OneWire_IsBusyLine(void);

/* Internal pin helpers */
void Base_Com_OneWire_DriveLow(Base_Com_OneWire_t *oneWire);
void Base_Com_OneWire_Release(Base_Com_OneWire_t *oneWire);

#ifdef __cplusplus
}
#endif

#endif /* BASE_COM_ONEWIRE_H_ */
