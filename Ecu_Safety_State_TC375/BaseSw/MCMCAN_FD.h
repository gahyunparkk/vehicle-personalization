#ifndef MCMCAN_FD_H_
#define MCMCAN_FD_H_

#include "Ifx_Types.h"
#include "IfxCan_Can.h"
#include "IfxPort.h"

#define CAN_BAUDRATE        500000
#define RX_MESSAGE_ID       0x100

typedef struct
{
    IfxCan_Can          canModule;
    IfxCan_Can_Node     canNode;
    /* MsgObj 핸들이 필요 없습니다! */
} mcmcanType;

void initMcmcan(void);
boolean receiveCanMessage(uint32 *rxData);

#endif
