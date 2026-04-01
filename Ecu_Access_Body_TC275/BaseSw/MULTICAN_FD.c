#include "MULTICAN_FD.h"

App_MulticanType g_multican;

void initMultican(void)
{
    IfxMultican_Can_Config canConfig;
    IfxMultican_Can_NodeConfig nodeConfig;
    IfxMultican_Can_MsgObjConfig canMsgObjConfig;

    /* 1. CAN 모듈 초기화 */
    IfxMultican_Can_initModuleConfig(&canConfig, &MODULE_CAN);
    IfxMultican_Can_initModule(&g_multican.can, &canConfig);

    /* 2. CAN 노드 초기화 */
    IfxMultican_Can_Node_initConfig(&nodeConfig, &g_multican.can);

    nodeConfig.nodeId = IfxMultican_NodeId_0;
    nodeConfig.baudrate = CAN_BAUDRATE; // 500kbps

    /* 핀 할당 */
    nodeConfig.rxPin = &IfxMultican_RXD0B_P20_7_IN;
    nodeConfig.rxPinMode = IfxPort_InputMode_pullUp;
    nodeConfig.txPin = &IfxMultican_TXD0_P20_8_OUT;
    nodeConfig.txPinMode = IfxPort_OutputMode_pushPull;
    nodeConfig.pinDriver = IfxPort_PadDriver_cmosAutomotiveSpeed1;

    IfxMultican_Can_Node_init(&g_multican.canNode0, &nodeConfig);

    /* 3. 송신 Message Object 초기화 (여기서 FD 설정을 합니다) */
    IfxMultican_Can_MsgObj_initConfig(&canMsgObjConfig, &g_multican.canNode0);

    canMsgObjConfig.msgObjId = 0;
    canMsgObjConfig.messageId = TX_MESSAGE_ID; // 0x100
    canMsgObjConfig.frame = IfxMultican_Frame_transmit;

    /* [핵심] CAN FD 및 BRS 활성화 설정 */
    canMsgObjConfig.control.messageLen = IfxMultican_DataLengthCode_8;
    canMsgObjConfig.control.fastBitRate = TRUE;    // <--- 가현님이 찾으신 BRS 스위치!
    canMsgObjConfig.control.extendedFrame = FALSE; // Standard ID(11-bit) 사용

    /* 중복 선언 제거 후 단 한번만 초기화 */
    IfxMultican_Can_MsgObj_init(&g_multican.canSrcMsgObj, &canMsgObjConfig);
}

void transmitCanMessage(uint8 *data)
{
    IfxMultican_Message txMsg;

    /* 8바이트 데이터 분할 할당 */
    uint32 low  = (uint32)data[0] | ((uint32)data[1] << 8) | ((uint32)data[2] << 16) | ((uint32)data[3] << 24);
    uint32 high = (uint32)data[4] | ((uint32)data[5] << 8) | ((uint32)data[6] << 16) | ((uint32)data[7] << 24);

    IfxMultican_Message_init(&txMsg, TX_MESSAGE_ID, low, high, IfxMultican_DataLengthCode_8);


    while(IfxMultican_Can_MsgObj_sendMessage(&g_multican.canSrcMsgObj, &txMsg) == IfxMultican_Status_notSentBusy);
}
