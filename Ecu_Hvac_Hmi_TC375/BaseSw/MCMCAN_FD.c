// MCMCAN_FD.C
#include "MCMCAN_FD.h"

McmcanType g_mcmcan;

/* 핀 설정 구조체 */
static const IfxCan_Can_Pins canPins =
{
    &TX_PIN, IfxPort_OutputMode_pushPull,
    &RX_PIN, IfxPort_InputMode_pullUp,
    IfxPort_PadDriver_cmosAutomotiveSpeed1
};


void initMcmcan(void)
{
    /* 1. [시동] 트랜시버 전원 및 CAN 모듈 클럭 활성화 */
    IfxPort_setPinModeOutput(&MODULE_P20, 6, IfxPort_OutputMode_pushPull, IfxPort_OutputIdx_general);
    IfxPort_setPinLow(&MODULE_P20, 6); // 트랜시버 켜기

    uint16 password = IfxScuWdt_getCpuWatchdogPassword();
    IfxScuWdt_clearCpuEndinit(password);
    MODULE_CAN0.CLC.B.DISR = 0;        // CAN 모듈에 전기 공급
    IfxScuWdt_setCpuEndinit(password);
    while(MODULE_CAN0.CLC.B.DISS);    // 완전히 깨어날 때까지 대기

    /* 2. [모듈] 모듈 레벨 초기화 (항상 노드 설정보다 먼저!) */
    IfxCan_Can_initModuleConfig(&g_mcmcan.canConfig, &MODULE_CAN0);
    IfxCan_Can_initModule(&g_mcmcan.canModule, &g_mcmcan.canConfig);

    /* 3. [노드] 노드 기본 설정 초기화 */
    IfxCan_Can_initNodeConfig(&g_mcmcan.canNodeConfig, &g_mcmcan.canModule);

    g_mcmcan.canNodeConfig.pins = &canPins;
    g_mcmcan.canNodeConfig.busLoopbackEnabled = FALSE;
    g_mcmcan.canNodeConfig.nodeId = IfxCan_NodeId_0;
    g_mcmcan.canNodeConfig.frame.type = IfxCan_FrameType_transmitAndReceive;
    g_mcmcan.canNodeConfig.frame.mode = IfxCan_FrameMode_fdLongAndFast;

    /* TX interrupt */
    g_mcmcan.canNodeConfig.interruptConfig.transmissionCompletedEnabled = TRUE;
    g_mcmcan.canNodeConfig.interruptConfig.traco.priority = ISR_PRIORITY_CAN_TX;
    g_mcmcan.canNodeConfig.interruptConfig.traco.interruptLine = IfxCan_InterruptLine_0;
    g_mcmcan.canNodeConfig.interruptConfig.traco.typeOfService = IfxSrc_Tos_cpu0;

    /* RX FIFO0 interrupt */
    g_mcmcan.canNodeConfig.interruptConfig.rxFifo0NewMessageEnabled = TRUE;
    g_mcmcan.canNodeConfig.interruptConfig.rxf0n.priority = ISR_PRIORITY_CAN_RX;
    g_mcmcan.canNodeConfig.interruptConfig.rxf0n.interruptLine = IfxCan_InterruptLine_1;
    g_mcmcan.canNodeConfig.interruptConfig.rxf0n.typeOfService = IfxSrc_Tos_cpu0;

    /* [속도] 500k / 5M 설정 */
    g_mcmcan.canNodeConfig.baudRate.baudrate = 500000;
    g_mcmcan.canNodeConfig.fastBaudRate.baudrate = 5000000;
    g_mcmcan.canNodeConfig.fastBaudRate.tranceiverDelayOffset = 12; // 지연 보상
    g_mcmcan.canNodeConfig.calculateBitTimingValues = TRUE;

    /* ⭐ [수정] 메모리 RAM 사이즈 예약 (매우 중요!) ⭐ */
    /* 이 값이 0이면 메모리가 할당되지 않아 수신 데이터가 계속 0으로 뜹니다. */
    g_mcmcan.canNodeConfig.filterConfig.standardListSize = 1;      // 필터 1개 사용
    g_mcmcan.canNodeConfig.rxConfig.rxFifo0Size = 16;             // FIFO 16칸 사용

    g_mcmcan.canNodeConfig.messageRAM.baseAddress = (uint32)&MODULE_CAN0;
    g_mcmcan.canNodeConfig.messageRAM.standardFilterListStartAddress = 0x100;
    g_mcmcan.canNodeConfig.messageRAM.rxFifo0StartAddress = 0x200;

    /* FD를 위해 주머니 크기를 64바이트로 통일 (8바이트 신호도 64 주머니에 담는 게 가장 안전함) */
    g_mcmcan.canNodeConfig.messageRAM.txBuffersStartAddress = 0x600;
    g_mcmcan.canNodeConfig.txConfig.txFifoQueueSize = 1;
    g_mcmcan.canNodeConfig.txConfig.dedicatedTxBuffersNumber = 1;
    g_mcmcan.canNodeConfig.txConfig.txBufferDataFieldSize = IfxCan_DataFieldSize_64; // ✅ FD는 반드시 64 지정
    g_mcmcan.canNodeConfig.rxConfig.rxFifo0DataFieldSize  = IfxCan_DataFieldSize_64; // RX도 동일하게

    /* ⭐ [안전장치] 필터에 안 맞아도 일단 FIFO 0로 받기 ⭐ */
    /* Enum 이름 에러 방지를 위해 숫자 0 (Accept)으로 강제 캐스팅 */
    g_mcmcan.canNodeConfig.filterConfig.standardFilterForNonMatchingFrames = (IfxCan_NonMatchingFrame)0;

    /* RX FIFO0 settings */
    g_mcmcan.canNodeConfig.rxConfig.rxMode = IfxCan_RxMode_fifo0;
    g_mcmcan.canNodeConfig.rxConfig.rxFifo0Size = 16;
    g_mcmcan.canNodeConfig.rxConfig.rxFifo0OperatingMode = IfxCan_RxFifoMode_blocking;

    /* 노드 초기화 실행 (여기서 위 설정들이 하드웨어에 구워짐) */
    IfxCan_Can_initNode(&g_mcmcan.canSrcNode, &g_mcmcan.canNodeConfig);

    /* 5. [필터] 0x100 수신용 필터 설정 */
    g_mcmcan.canFilter.number = 0;
    g_mcmcan.canFilter.elementConfiguration = IfxCan_FilterElementConfiguration_storeInRxFifo0;
    g_mcmcan.canFilter.type = IfxCan_FilterType_range;
    g_mcmcan.canFilter.id1 = 0x100;
    g_mcmcan.canFilter.id2 = 0x500;

    IfxCan_Can_setStandardFilter(&g_mcmcan.canSrcNode, &g_mcmcan.canFilter);

    /* 동기화 대기 */
    while (IfxCan_Can_isNodeSynchronized(&g_mcmcan.canSrcNode) != TRUE);
}

/* [송신] 기존 함수 유지 */
void transmitCanMessage(uint32 txId, uint32 *pData)
{
    IfxCan_Can_initMessage(&g_mcmcan.txMsg);
    g_mcmcan.txMsg.frameMode = IfxCan_FrameMode_fdLongAndFast;
    g_mcmcan.txMsg.dataLengthCode = IfxCan_DataLengthCode_64;
    g_mcmcan.txMsg.messageId = txId;

    /* 64바이트 상자(txData)에 데이터 복사 */
    for(int i = 0; i < 16; i++)
    {
        /* pData가 가리키는 곳에서 16개를 가져와 꽉 채웁니다 */
        g_mcmcan.txData[i] = pData[i];
    }
    while( IfxCan_Status_notSentBusy ==
           IfxCan_Can_sendMessage(&g_mcmcan.canSrcNode, &g_mcmcan.txMsg, &g_mcmcan.txData[0]) );
}

/* ⭐ [수신] TC275 스타일의 폴링 수신 함수 ⭐ */
boolean receiveCanMessage(uint32 *rxData)
{
    /* Rx FIFO 0에 메시지가 쌓여있는지 레벨 체크 */
    if (IfxCan_Can_getRxFifo0FillLevel(&g_mcmcan.canSrcNode) > 0)
    {
        IfxCan_Message rxMsg;
        IfxCan_Can_initMessage(&rxMsg);
        rxMsg.readFromRxFifo0 = TRUE;

        /* FIFO에서 데이터를 긁어서 rxData 배열에 저장 */
        IfxCan_Can_readMessage(&g_mcmcan.canSrcNode, &rxMsg, rxData);

        /* g_mcmcan 구조체 내부 버퍼에도 동기화 */
        for(int i = 0; i < 16; i++)
        {
            g_mcmcan.rxData[i] = rxData[i];
        }
        return TRUE; /* 수신 성공! */
    }

    return FALSE; /* 아직 들어온 데이터 없음 */
}
