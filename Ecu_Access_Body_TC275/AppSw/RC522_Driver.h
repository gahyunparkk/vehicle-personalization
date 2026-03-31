#ifndef RC522_DRIVER_H_
#define RC522_DRIVER_H_

#include "Platform_Types.h"

#define PICC_REQIDL    0x26
#define PICC_ANTICOLL  0x93
#define STATUS_OK      0x00

void RC522_Init(void);
uint8 RC522_Request(uint8 reqMode, uint8 *TagType);
uint8 RC522_Anticoll(uint8 *serNum);

#endif
