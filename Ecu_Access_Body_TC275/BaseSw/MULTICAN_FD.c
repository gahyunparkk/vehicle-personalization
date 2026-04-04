#include "MULTICAN_FD.h"

multicanType g_multican;

void initMultican(void)
{
    /* 트랜시버 전원 활성화 */
    IfxPort_setPinModeOutput(&MODULE_P20, 6, IfxPort_OutputMode_pushPull, IfxPort_OutputIdx_general);
    IfxPort_setPinLow(&MODULE_P20, 6);

    /* CAN 모듈 초기화 */
    IfxMultican_Can_initModuleConfig(&g_multican.canConfig, &MODULE_CAN);
    IfxMultican_Can_initModule(&g_multican.can, &g_multican.canConfig);

    /* CAN 노드 설정 (500k / 5M FD 설정) */
    IfxMultican_Can_Node_initConfig(&g_multican.nodeConfig, &g_multican.can);

    g_multican.nodeConfig.nodeId = IfxMultican_NodeId_0;
    g_multican.nodeConfig.rxPin = &IfxMultican_RXD0B_P20_7_IN;
    g_multican.nodeConfig.txPin = &IfxMultican_TXD0_P20_8_OUT;
    g_multican.nodeConfig.flexibleDataRate = TRUE;

    g_multican.nodeConfig.fdConfig.nominalBaudrate = 500000;
    g_multican.nodeConfig.fdConfig.fastBaudrate = 5000000;
    g_multican.nodeConfig.fdConfig.fastSamplePoint = 7500;
    g_multican.nodeConfig.fdConfig.loopDelayOffset = 12;

    IfxMultican_Can_Node_init(&g_multican.canNode0, &g_multican.nodeConfig);

    /* 수신 객체 초기화 */
    IfxMultican_Can_MsgObj_initConfig(&g_multican.canMsgObjConfig, &g_multican.canNode0);
    g_multican.canMsgObjConfig.msgObjId = 1;                /* 수신용 1번 */
    g_multican.canMsgObjConfig.messageId = RX_MESSAGE_ID;
    g_multican.canMsgObjConfig.frame = IfxMultican_Frame_receive;
    g_multican.canMsgObjConfig.control.messageLen = IfxMultican_DataLengthCode_64;
    g_multican.canMsgObjConfig.control.fastBitRate = TRUE;

    IfxMultican_Can_MsgObj_init(&g_multican.canRxMsgObj, &g_multican.canMsgObjConfig);

    /* 송신 객체 초기화 */
    IfxMultican_Can_MsgObj_initConfig(&g_multican.canMsgObjConfig, &g_multican.canNode0);
    g_multican.canMsgObjConfig.msgObjId = 0;                /* 송신용 0번 */
    g_multican.canMsgObjConfig.messageId = TX_MESSAGE_ID;
    g_multican.canMsgObjConfig.frame = IfxMultican_Frame_transmit;
    g_multican.canMsgObjConfig.control.fastBitRate = TRUE;

    IfxMultican_Can_MsgObj_init(&g_multican.canSrcMsgObj, &g_multican.canMsgObjConfig);

    /* 하드웨어 레지스터 직접 접근하여 FD 활성화 */
    MODULE_CAN.MO[g_multican.canSrcMsgObj.msgObjId].FCR.B.FDF = 1;
    MODULE_CAN.MO[g_multican.canSrcMsgObj.msgObjId].FCR.B.BRS = 1;
}

/* 송신 함수 */
void transmitCanMessage(uint32 txId, uint32 *pData)
{
    /* 구조체 내부의 txMsg 초기화 및 BRS 도장 찍기 */
    IfxMultican_Message_init(&g_multican.txMsg, txId, 0, 0, IfxMultican_DataLengthCode_64);
    g_multican.txMsg.fastBitRate = TRUE;

    for(int i = 0; i < 16; i++)
    {
        g_multican.txMsg.data[i] = pData[i];
    }

    /* 전송 시도 */
    while(IfxMultican_Status_notSentBusy ==
          IfxMultican_Can_MsgObj_sendMessage(&g_multican.canSrcMsgObj, &g_multican.txMsg));
}

/*수신*/
boolean receiveCanMessage(uint32 *rxData)
{
    /* 구조체 내부의 rxMsg 사용 */
    IfxMultican_Message_init(&g_multican.rxMsg, 0, 0, 0, IfxMultican_DataLengthCode_64);

    /* NEWDAT 플래그 확인 (폴링 방식) */
    if (IfxMultican_Can_MsgObj_readMessage(&g_multican.canRxMsgObj, &g_multican.rxMsg) == IfxMultican_Status_newData)
    {
        /* 받은 데이터를 구조체 버퍼 및 인자 배열에 복사 */
        for(int i = 0; i < 16; i++)
        {
            g_multican.rxData[i] = g_multican.rxMsg.data[i];
            rxData[i] = g_multican.rxData[i];
        }
        return TRUE;
    }

    return FALSE;
}
