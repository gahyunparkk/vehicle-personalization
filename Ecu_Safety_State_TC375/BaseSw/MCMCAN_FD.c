#include "MCMCAN_FD.h"
#include <string.h>

mcmcanType g_mcmcan;

/* [추가] 통신 성공 횟수를 저장하는 카운터 (디버거 Variables 창에서 확인용) */
volatile uint32 g_rxSuccessCount = 0;

void initMcmcan(void)
{
    /* 1. 트랜시버 깨우기 (P20.6 Low) */
    IfxPort_setPinModeOutput(&MODULE_P20, 6, IfxPort_OutputMode_pushPull, IfxPort_OutputIdx_general);
    IfxPort_setPinLow(&MODULE_P20, 6);

    IfxCan_Can_Config canConfig;
    IfxCan_Can_NodeConfig nodeConfig;

    /* 2. CAN 모듈 초기화 */
    IfxCan_Can_initModuleConfig(&canConfig, &MODULE_CAN0);
    IfxCan_Can_initModule(&g_mcmcan.canModule, &canConfig);

    /* 3. CAN 노드 설정 */
    IfxCan_Can_initNodeConfig(&nodeConfig, &g_mcmcan.canModule);

    nodeConfig.nodeId = IfxCan_NodeId_0;
    nodeConfig.baudRate.baudrate = CAN_BAUDRATE;
    nodeConfig.calculateBitTimingValues = TRUE;

    /* [핵심 수정] 275 송신기에 맞춰 FD 모드로 설정 */
    /* 만약 275에서 Bit Rate Switch를 켰다면 IfxCan_FrameMode_fdLongAndFast를 사용하세요 */
    nodeConfig.frame.mode = IfxCan_FrameMode_fdLongAndFast;
    /* [핵심] 모든 ID를 수용하는 만능 필터 설정 */
    nodeConfig.filterConfig.standardListSize = 0;
    nodeConfig.filterConfig.standardFilterForNonMatchingFrames = IfxCan_NonMatchingFrame_acceptToRxFifo0;

    /* Message RAM 주소 배치 */
    nodeConfig.rxConfig.rxMode = IfxCan_RxMode_sharedFifo0;
    nodeConfig.rxConfig.rxFifo0Size = 32;
    nodeConfig.rxConfig.rxFifo0DataFieldSize = IfxCan_DataFieldSize_8;

    nodeConfig.messageRAM.standardFilterListStartAddress = 0x0;
    nodeConfig.messageRAM.rxFifo0StartAddress = 0x100;

    const IfxCan_Can_Pins pins = {
        &IfxCan_TXD00_P20_8_OUT, IfxPort_OutputMode_pushPull,
        &IfxCan_RXD00B_P20_7_IN, IfxPort_InputMode_pullUp,
        IfxPort_PadDriver_cmosAutomotiveSpeed1
    };
    nodeConfig.pins = &pins;

    /* 노드 초기화 실행 */
    IfxCan_Can_initNode(&g_mcmcan.canNode, &nodeConfig);

    /* 버스 동기화 대기 */
    while (IfxCan_Can_isNodeSynchronized(&g_mcmcan.canNode) != TRUE);
}

boolean receiveCanMessage(uint32 *rxData)
{
    /* [체크] N/A 방지를 위해 volatile 사용 */
    volatile uint8 fillLevel = IfxCan_Can_getRxFifo0FillLevel(&g_mcmcan.canNode);

    if (fillLevel > 0)
    {
        IfxCan_Message rxMsg;
        IfxCan_Can_initMessage(&rxMsg);
        rxMsg.readFromRxFifo0 = TRUE;

        /* 데이터를 실제로 읽어옴 */
        IfxCan_Can_readMessage(&g_mcmcan.canNode, &rxMsg, rxData);

        /* [핵심 추가] 수신 성공 시 카운트 증가 */
        g_rxSuccessCount++;

        return TRUE;
    }
    return FALSE;
}
