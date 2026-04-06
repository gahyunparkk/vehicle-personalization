#ifndef MULTICAN_FD_H
#define MULTICAN_FD_H

#include "Ifx_Types.h"
#include "IfxMultican_Can.h"
#include "IfxMultican.h"
#include "IfxPort.h"

/*********************************************************************************************************************/
/*------------------------------------------------------Macros-------------------------------------------------------*/
/*********************************************************************************************************************/
/*
 * RX interrupt
 * - 현재 polling 기반으로 써도 되고
 * - 이후 RX 인터럽트 확장 시 그대로 사용 가능
 */
#define ISR_PRIORITY_CAN_RX                 (1U)
#define RX_INTERRUPT_SRC_ID                 (IfxMultican_SrcId_0)

/*
 * CAN FD payload work buffer
 * - uint32[16] = 64 bytes
 */
#define MAXIMUM_CAN_DATA_PAYLOAD            (16U)
#define MULTICAN_FD_PAYLOAD_SIZE_BYTE       (64U)

/*
 * Message Object ID
 * - 64B CAN FD long frame용으로 base/top/bottom message object를 연결해서 사용
 */
#define MULTICAN_TX_BASE_MSGOBJ_ID          (0U)
#define MULTICAN_RX_BASE_MSGOBJ_ID          (1U)

#define MULTICAN_TX_TOP_MSGOBJ_ID           (64U)
#define MULTICAN_TX_BOTTOM_MSGOBJ_ID        (65U)
#define MULTICAN_RX_TOP_MSGOBJ_ID           (66U)
#define MULTICAN_RX_BOTTOM_MSGOBJ_ID        (67U)

/*********************************************************************************************************************/
/*------------------------------------------------Type Definitions--------------------------------------------------*/
/*********************************************************************************************************************/
typedef struct
{
    IfxMultican_Can               can;             /* CAN module handle */
    IfxMultican_Can_Config        canConfig;       /* CAN module config */

    IfxMultican_Can_Node          canNode0;        /* CAN node handle */
    IfxMultican_Can_NodeConfig    nodeConfig;      /* CAN node config */

    IfxMultican_Can_MsgObj        canSrcMsgObj;    /* TX base message object */
    IfxMultican_Can_MsgObj        canRxMsgObj;     /* RX base message object */
    IfxMultican_Can_MsgObjConfig  canMsgObjConfig; /* Message object config */

    IfxMultican_Message           txMsg;           /* TX message */
    IfxMultican_Message           rxMsg;           /* RX message */

    uint32                        txData[MAXIMUM_CAN_DATA_PAYLOAD]; /* TX data buffer (64B) */
    uint32                        rxData[MAXIMUM_CAN_DATA_PAYLOAD]; /* RX data buffer (64B) */
} multicanType;

extern multicanType g_multican;

/*********************************************************************************************************************/
/*------------------------------------------------Function Prototypes------------------------------------------------*/
/*********************************************************************************************************************/
void    initMultican(void);
void    transmitCanMessage(uint32 txId, uint32 *pData);
boolean receiveCanMessage(uint32 *rxData);

#endif /* MULTICAN_FD_H */