#ifndef BASE_DRIVER_MFRC522_H_
#define BASE_DRIVER_MFRC522_H_

/*********************************************************************************************************************/
/*-----------------------------------------------------Includes------------------------------------------------------*/
/*********************************************************************************************************************/
#include "Platform_Types.h"

/*********************************************************************************************************************/
/*------------------------------------------------Type Definitions--------------------------------------------------*/
/*********************************************************************************************************************/
typedef enum
{
    MFRC522_STATUS_OK = 0,
    MFRC522_STATUS_ERROR,
    MFRC522_STATUS_TIMEOUT,
    MFRC522_STATUS_NO_TAG
} Mfrc522_Status;

typedef struct
{
    uint8 size;
    uint8 uid[4];
} Mfrc522_Uid;

/*********************************************************************************************************************/
/*------------------------------------------------Function Prototypes------------------------------------------------*/
/*********************************************************************************************************************/
boolean        Base_Driver_Mfrc522_Init(void);
uint8          Base_Driver_Mfrc522_ReadVersion(void);

uint8          Base_Driver_Mfrc522_ReadReg(uint8 reg);
void           Base_Driver_Mfrc522_WriteReg(uint8 reg, uint8 value);
void           Base_Driver_Mfrc522_SetBitMask(uint8 reg, uint8 mask);
void           Base_Driver_Mfrc522_ClearBitMask(uint8 reg, uint8 mask);

Mfrc522_Status Base_Driver_Mfrc522_RequestA(uint8 atqa[2]);
Mfrc522_Status Base_Driver_Mfrc522_WakeupA(uint8 atqa[2]);
boolean        Base_Driver_Mfrc522_IsNewCardPresent(void);
Mfrc522_Status Base_Driver_Mfrc522_ReadUid(Mfrc522_Uid *outUid);

#endif /* BASE_DRIVER_MFRC522_H_ */
