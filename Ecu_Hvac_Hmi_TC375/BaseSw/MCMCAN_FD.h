#ifndef MCMCAN_FD_H
#define MCMCAN_FD_H

#include "Ifx_Types.h"
#include "Can/Can/IfxCan_Can.h"
//#include "Can/Can/IfxCan.h"
#include "IfxPort.h"
#include "IfxScuWdt.h"
#include <string.h>

/* 매크로 설정 */
#define TX_PIN                  IfxCan_TXD00_P20_8_OUT
#define RX_PIN                  IfxCan_RXD00B_P20_7_IN
#define ISR_PRIORITY_CAN_TX     2
#define ISR_PRIORITY_CAN_RX     1
#define MAXIMUM_CAN_DATA_PAYLOAD 16      /* uint32 x 16 = 64 Bytes (FD) */
#define INVALID_RX_DATA_VALUE   0xAAAAAAAA

/* 구조체 정의 */
typedef struct
{
    IfxCan_Can_Config       canConfig;
    IfxCan_Can              canModule;
    IfxCan_Can_Node         canSrcNode;         /* 메인 통신 노드 */
    IfxCan_Can_NodeConfig   canNodeConfig;
    IfxCan_Filter           canFilter;
    IfxCan_Message          txMsg;
    IfxCan_Message          rxMsg;
    uint32                  txData[MAXIMUM_CAN_DATA_PAYLOAD];
    uint32                  rxData[MAXIMUM_CAN_DATA_PAYLOAD];
} McmcanType;

/* 전역 변수 및 함수 선언 */
extern McmcanType g_mcmcan;

void initMcmcan(void);
void transmitCanMessage(uint32 txId, uint32 *pData);
boolean receiveCanMessage(uint32 *rxData);

#endif
