#ifndef BASE_RC522_H_
#define BASE_RC522_H_

#include "Ifx_Types.h"

typedef enum
{
    RC522_STATUS_OK = 0,
    RC522_STATUS_TIMEOUT,
    RC522_STATUS_ERROR,
    RC522_STATUS_NO_TAG
} Rc522_Status;

typedef struct
{
    uint8 uid[10];
    uint8 size;   /* 4, 7, 10 */
    uint8 sak;
} Rc522_Uid;

boolean      RC522_Init(void);
uint8        RC522_ReadVersion(void);

Rc522_Status RC522_RequestA(uint8 atqa[2]);
boolean      RC522_IsNewCardPresent(void);

/* Raw register helpers */
uint8        RC522_ReadReg(uint8 reg);
void         RC522_WriteReg(uint8 reg, uint8 value);
void         RC522_SetBitMask(uint8 reg, uint8 mask);
void         RC522_ClearBitMask(uint8 reg, uint8 mask);
Rc522_Status RC522_ReadUid(Rc522_Uid *outUid);
Rc522_Status RC522_WakeupA(uint8 atqa[2]);

#endif /* BASE_RC522_H_ */
