#ifndef MULTICAN_FD_H
#define MULTICAN_FD_H

#include "Ifx_Types.h"
#include "IfxMultican_Can.h"
#include "IfxMultican.h"
#include "IfxPort.h"

/* 인터럽트 및 기본 ID 설정 */
#define ISR_PRIORITY_CAN_RX         1
#define RX_INTERRUPT_SRC_ID         IfxMultican_SrcId_0
#define TX_MESSAGE_ID               0x100   /* 기본 송신 ID */
#define RX_MESSAGE_ID               0x150   /* 375로부터 받을 ID */
#define MAXIMUM_CAN_DATA_PAYLOAD    16

/* ⭐ TC375 스타일의 통합 구조체 정의 ⭐ */
typedef struct
{
    IfxMultican_Can             can;                /* 모듈 핸들 */
    IfxMultican_Can_Config      canConfig;          /* 모듈 설정 */
    IfxMultican_Can_Node        canNode0;           /* 노드 핸들 */
    IfxMultican_Can_NodeConfig  nodeConfig;         /* 노드 설정 */
    IfxMultican_Can_MsgObj      canSrcMsgObj;       /* 송신 객체 */
    IfxMultican_Can_MsgObj      canRxMsgObj;        /* 수신 객체 */
    IfxMultican_Can_MsgObjConfig canMsgObjConfig;   /* 객체 설정 */
    IfxMultican_Message         txMsg;              /* 송신 메시지 구조체 */
    IfxMultican_Message         rxMsg;              /* 수신 메시지 구조체 */
    uint32                      txData[MAXIMUM_CAN_DATA_PAYLOAD];          /* 송신 데이터 버퍼 */
    uint32                      rxData[MAXIMUM_CAN_DATA_PAYLOAD];          /* 수신 데이터 버퍼 */
} multicanType;

extern multicanType g_multican;

/* 함수 프로토타입 */
void initMultican(void);
void transmitCanMessage(uint32 txId, uint32 *pData);
boolean receiveCanMessage(uint32 *rxData);

#endif
