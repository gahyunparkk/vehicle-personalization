#ifndef ONEWIRE_H
#define ONEWIRE_H

#include "Platform_Types.h"
#include "IfxPort.h"

typedef struct {
        Ifx_P*  port;
        uint8   pinIndex;
} Gpio_Struct;

typedef struct {
    Gpio_Struct DQ;
    uint8       LastDiscrepancy;       /*!< Search private */
    uint8       LastFamilyDiscrepancy; /*!< Search private */
    uint8       LastDeviceFlag;        /*!< Search private */
    uint8       ROM_NO[8];             /*!< 8-bytes address of last search device */
} OneWire_t;

/* OneWire delay */
void ONEWIRE_DELAY(uint16 time_us);

/* Pin settings */
void ONEWIRE_LOW(OneWire_t *gp);
void ONEWIRE_HIGH(OneWire_t *gp);
void ONEWIRE_INPUT(OneWire_t *gp);
void ONEWIRE_OUTPUT(OneWire_t *gp);

/* OneWire commands */
#define ONEWIRE_CMD_RSCRATCHPAD         0xBE
#define ONEWIRE_CMD_WSCRATCHPAD         0x4E
#define ONEWIRE_CMD_CPYSCRATCHPAD       0x48
#define ONEWIRE_CMD_RECEEPROM           0xB8
#define ONEWIRE_CMD_RPWRSUPPLY          0xB4
#define ONEWIRE_CMD_SEARCHROM           0xF0
#define ONEWIRE_CMD_READROM             0x33
#define ONEWIRE_CMD_MATCHROM            0x55
#define ONEWIRE_CMD_SKIPROM             0xCC

//#######################################################################################################
void  OneWire_Init(OneWire_t* OneWireStruct, Ifx_P* port, uint8 pinIndex);
uint8 OneWire_Reset(OneWire_t* OneWireStruct);
uint8 OneWire_ReadByte(OneWire_t* OneWireStruct);
void  OneWire_WriteByte(OneWire_t* OneWireStruct, uint8 byte);
void  OneWire_WriteBit(OneWire_t* OneWireStruct, uint8 bit);
uint8 OneWire_ReadBit(OneWire_t* OneWireStruct);
uint8 OneWire_Search(OneWire_t* OneWireStruct, uint8 command);
void  OneWire_ResetSearch(OneWire_t* OneWireStruct);
uint8 OneWire_First(OneWire_t* OneWireStruct);
uint8 OneWire_Next(OneWire_t* OneWireStruct);
void  OneWire_GetFullROM(OneWire_t* OneWireStruct, uint8 *firstIndex);
void  OneWire_Select(OneWire_t* OneWireStruct, uint8* addr);
void  OneWire_SelectWithPointer(OneWire_t* OneWireStruct, uint8* ROM);
uint8 OneWire_CRC8(uint8* addr, uint8 len);
uint8 OneWire_isBusyLine();


void  ONEWIRE_DELAY(uint16 time_us);
void  ONEWIRE_DRIVE_LOW(OneWire_t* gp);
void  ONEWIRE_RELEASE(OneWire_t* gp);

//#######################################################################################################

/* C++ detection */
#ifdef __cplusplus
}
#endif

#endif

