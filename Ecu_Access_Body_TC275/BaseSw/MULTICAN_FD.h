#ifndef MULTICAN_FD_H_
#define MULTICAN_FD_H_

#include "Ifx_Types.h"
#include "IfxMultican_Can.h"
#include "IfxMultican.h"
#include "IfxPort.h"

#define CAN_BAUDRATE        500000              /* 500kbps */
#define TX_MESSAGE_ID       0x100               /* 송신 ID */

typedef struct
{
    IfxMultican_Can          can;               /* CAN handle */
    IfxMultican_Can_Node     canNode0;          /* CAN node handle */
    IfxMultican_Can_MsgObj   canSrcMsgObj;      /* CAN message object handle */
} App_MulticanType;

/* 함수 선언 */
void initMultican(void);
void transmitCanMessage(uint8 *data);

#endif /* MULTICAN_FD_H_ */
