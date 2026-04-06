#include "MCMCAN_FD.h"
#include "Shared_Can_Message.h"
#include "UART_Config.h"

#include <string.h>

McmcanType g_mcmcan;

/* 핀 설정 */
static const IfxCan_Can_Pins g_mcmcan_pins =
{
    &TX_PIN,
    IfxPort_OutputMode_pushPull,
    &RX_PIN,
    IfxPort_InputMode_pullUp,
    IfxPort_PadDriver_cmosAutomotiveSpeed1
};

static const char *Mcmcan_GetProfileTableRxName(uint32 message_id)
{
    switch (message_id)
    {
        case SHARED_CAN_MSG_ID_SS_PROFILE_TABLE:
        {
            return "SS_PT";
        }

        case SHARED_CAN_MSG_ID_AB_PROFILE_TABLE:
        {
            return "AB_PT";
        }

        case SHARED_CAN_MSG_ID_HH_PROFILE_TABLE:
        {
            return "HH_PT";
        }

        default:
        {
            return NULL_PTR;
        }
    }
}

static void Mcmcan_LogProfileTableRxCompact(uint32        message_id,
                                            const uint32 *rx_data)
{
    static const char hex_chars[] = "0123456789ABCDEF";
    const char       *rx_name;
    const uint8      *raw_bytes;
    char              line[120];
    uint8             line_idx;
    uint8             byte_idx;

    if (rx_data == NULL_PTR)
    {
        return;
    }

    rx_name = Mcmcan_GetProfileTableRxName(message_id);
    if (rx_name == NULL_PTR)
    {
        return;
    }

    raw_bytes = (const uint8 *)rx_data;
    line_idx  = 0U;

    line[line_idx++] = '[';
    line[line_idx++] = 'M';
    line[line_idx++] = 'C';
    line[line_idx++] = 'M';
    line[line_idx++] = ']';
    line[line_idx++] = '[';
    line[line_idx++] = 'R';
    line[line_idx++] = 'X';
    line[line_idx++] = ']';
    line[line_idx++] = ' ';

    while ((*rx_name != '\0') && (line_idx < (uint8)(sizeof(line) - 1U)))
    {
        line[line_idx++] = *rx_name;
        rx_name++;
    }

    if (line_idx < (uint8)(sizeof(line) - 1U))
    {
        line[line_idx++] = ' ';
    }

    for (byte_idx = 0U; byte_idx < SHARED_CAN_MSG_SIZE_PROFILE_TABLE; ++byte_idx)
    {
        if ((byte_idx != 0U) && ((byte_idx % SHARED_PROFILE_SIZE_BYTE) == 0U))
        {
            if (line_idx >= (uint8)(sizeof(line) - 2U))
            {
                break;
            }

            line[line_idx++] = '|';
        }

        if (line_idx >= (uint8)(sizeof(line) - 3U))
        {
            break;
        }

        line[line_idx++] = hex_chars[(raw_bytes[byte_idx] >> 4U) & 0x0FU];
        line[line_idx++] = hex_chars[raw_bytes[byte_idx] & 0x0FU];
    }

    line[line_idx] = '\0';

    UART_Printf("%s\r\n", line);
}

void initMcmcan(void)
{
    uint16 password;

    /* ------------------------------------------------------------------------------------------------------------- */
    /* 1. CAN 트랜시버 활성화                                                                                        */
    /* 보드에 따라 P20.6 LOW/HIGH 극성이 다를 수 있음                                                               */
    /* 지금은 네 기존 코드 기준으로 LOW = enable 로 둠                                                               */
    /* ------------------------------------------------------------------------------------------------------------- */
    IfxPort_setPinModeOutput(&MODULE_P20, 6, IfxPort_OutputMode_pushPull, IfxPort_OutputIdx_general);
    IfxPort_setPinLow(&MODULE_P20, 6);

    /* ------------------------------------------------------------------------------------------------------------- */
    /* 2. CAN 모듈 클럭 공급                                                                                         */
    /* ------------------------------------------------------------------------------------------------------------- */
    password = IfxScuWdt_getCpuWatchdogPassword();
    IfxScuWdt_clearCpuEndinit(password);
    MODULE_CAN0.CLC.B.DISR = 0U;
    IfxScuWdt_setCpuEndinit(password);

    while (MODULE_CAN0.CLC.B.DISS != 0U)
    {
        /* wait */
    }

    /* ------------------------------------------------------------------------------------------------------------- */
    /* 3. MCMCAN 모듈 초기화                                                                                         */
    /* ------------------------------------------------------------------------------------------------------------- */
    IfxCan_Can_initModuleConfig(&g_mcmcan.canConfig, &MODULE_CAN0);
    IfxCan_Can_initModule(&g_mcmcan.canModule, &g_mcmcan.canConfig);

    /* ------------------------------------------------------------------------------------------------------------- */
    /* 4. Node 설정                                                                                                  */
    /* ------------------------------------------------------------------------------------------------------------- */
    IfxCan_Can_initNodeConfig(&g_mcmcan.canNodeConfig, &g_mcmcan.canModule);

    g_mcmcan.canNodeConfig.nodeId = IfxCan_NodeId_0;
    g_mcmcan.canNodeConfig.pins   = &g_mcmcan_pins;

    /* 외부 버스 사용 기준 */
    g_mcmcan.canNodeConfig.busLoopbackEnabled = FALSE;

    /* TX/RX 둘 다 사용 */
    g_mcmcan.canNodeConfig.frame.type = IfxCan_FrameType_transmitAndReceive;
    g_mcmcan.canNodeConfig.frame.mode = IfxCan_FrameMode_fdLongAndFast;

    /* ------------------------------------------------------------------------------------------------------------- */
    /* 5. 인터럽트는 전부 사용하지 않음 (폴링 전용)                                                                   */
    /* ------------------------------------------------------------------------------------------------------------- */
    g_mcmcan.canNodeConfig.interruptConfig.transmissionCompletedEnabled = FALSE;
    g_mcmcan.canNodeConfig.interruptConfig.rxFifo0NewMessageEnabled     = FALSE;

    /* ------------------------------------------------------------------------------------------------------------- */
    /* 6. CAN / CAN FD baud 설정                                                                                     */
    /* arbitration: 500 kbps, data: 5 Mbps                                                                          */
    /* ------------------------------------------------------------------------------------------------------------- */
    g_mcmcan.canNodeConfig.baudRate.baudrate = 500000U;

    g_mcmcan.canNodeConfig.fastBaudRate.baudrate               = 5000000U;
    g_mcmcan.canNodeConfig.fastBaudRate.tranceiverDelayOffset  = 12U;
    g_mcmcan.canNodeConfig.calculateBitTimingValues            = TRUE;

    /* ------------------------------------------------------------------------------------------------------------- */
    /* 7. Message RAM layout                                                                                         */
    /* offset 단위로 배치                                                                                           */
    /* ------------------------------------------------------------------------------------------------------------- */
    g_mcmcan.canNodeConfig.messageRAM.baseAddress                      = (uint32)&MODULE_CAN0;
    g_mcmcan.canNodeConfig.messageRAM.standardFilterListStartAddress   = 0x100U;
    g_mcmcan.canNodeConfig.messageRAM.rxFifo0StartAddress              = 0x200U;
    g_mcmcan.canNodeConfig.messageRAM.txBuffersStartAddress            = 0x600U;

    /* ------------------------------------------------------------------------------------------------------------- */
    /* 8. Filter / RX FIFO / TX Buffer                                                                               */
    /* ------------------------------------------------------------------------------------------------------------- */
    g_mcmcan.canNodeConfig.filterConfig.standardListSize = 1U;

    g_mcmcan.canNodeConfig.rxConfig.rxMode                  = IfxCan_RxMode_fifo0;
    g_mcmcan.canNodeConfig.rxConfig.rxFifo0Size             = 16U;
    g_mcmcan.canNodeConfig.rxConfig.rxFifo0OperatingMode    = IfxCan_RxFifoMode_blocking;
    g_mcmcan.canNodeConfig.rxConfig.rxFifo0DataFieldSize    = IfxCan_DataFieldSize_64;

    g_mcmcan.canNodeConfig.txConfig.txFifoQueueSize           = 1U;
    g_mcmcan.canNodeConfig.txConfig.dedicatedTxBuffersNumber  = 1U;
    g_mcmcan.canNodeConfig.txConfig.txBufferDataFieldSize     = IfxCan_DataFieldSize_64;

    /* 필터 미스매치 frame 처리:
       iLLD 버전에 따라 enum 이름 차이 있을 수 있어서 네 코드 스타일 유지 */
    g_mcmcan.canNodeConfig.filterConfig.standardFilterForNonMatchingFrames = (IfxCan_NonMatchingFrame)0;

    /* ------------------------------------------------------------------------------------------------------------- */
    /* 9. Node 초기화                                                                                                */
    /* ------------------------------------------------------------------------------------------------------------- */
    IfxCan_Can_initNode(&g_mcmcan.canNode, &g_mcmcan.canNodeConfig);

    /* ------------------------------------------------------------------------------------------------------------- */
    /* 10. Standard filter 설정                                                                                      */
    /* 0x100 ~ 0x500 범위 수신                                                                                       */
    /* ------------------------------------------------------------------------------------------------------------- */
    g_mcmcan.canFilter.number                = 0U;
    g_mcmcan.canFilter.elementConfiguration  = IfxCan_FilterElementConfiguration_storeInRxFifo0;
    g_mcmcan.canFilter.type                  = IfxCan_FilterType_range;
    g_mcmcan.canFilter.id1                   = 0x100U;
    g_mcmcan.canFilter.id2                   = 0x500U;

    IfxCan_Can_setStandardFilter(&g_mcmcan.canNode, &g_mcmcan.canFilter);

    /* ------------------------------------------------------------------------------------------------------------- */
    /* 11. 내부 데이터 초기화                                                                                         */
    /* ------------------------------------------------------------------------------------------------------------- */
    {
        uint8 i;
        for (i = 0U; i < MAXIMUM_CAN_DATA_PAYLOAD; ++i)
        {
            g_mcmcan.txData[i] = 0U;
            g_mcmcan.rxData[i] = 0U;
        }
    }

    /* 외부 버스에서 동작 시 보통 idle bus면 sync됨
       여기서 무한대기 넣고 싶으면 아래 사용
       while (IfxCan_Can_isNodeSynchronized(&g_mcmcan.canNode) != TRUE) {}
    */
}

boolean transmitCanMessage(uint32 txId, const uint32 *pData)
{
    uint8 idx;

    if (pData == NULL_PTR)
    {
        return FALSE;
    }

    IfxCan_Can_initMessage(&g_mcmcan.txMsg);

    g_mcmcan.txMsg.messageId      = txId;
    g_mcmcan.txMsg.frameMode      = IfxCan_FrameMode_fdLongAndFast;
    g_mcmcan.txMsg.dataLengthCode = IfxCan_DataLengthCode_64;

    for (idx = 0U; idx < MAXIMUM_CAN_DATA_PAYLOAD; ++idx)
    {
        g_mcmcan.txData[idx] = pData[idx];
    }

    while (IfxCan_Status_notSentBusy ==
           IfxCan_Can_sendMessage(&g_mcmcan.canNode, &g_mcmcan.txMsg, &g_mcmcan.txData[0]))
    {
        /* polling */
    }

    return TRUE;
}

boolean receiveCanMessage(uint32 *rxData)
{
    uint8 idx;

    if (rxData == NULL_PTR)
    {
        return FALSE;
    }

    /* RX FIFO0에 데이터 없으면 수신 실패 */
    if (IfxCan_Can_getRxFifo0FillLevel(&g_mcmcan.canNode) == 0U)
    {
        return FALSE;
    }

    IfxCan_Can_initMessage(&g_mcmcan.rxMsg);
    g_mcmcan.rxMsg.readFromRxFifo0 = TRUE;

    (void)IfxCan_Can_readMessage(&g_mcmcan.canNode,
                                 &g_mcmcan.rxMsg,
                                 rxData);

    for (idx = 0U; idx < MAXIMUM_CAN_DATA_PAYLOAD; ++idx)
    {
        g_mcmcan.rxData[idx] = rxData[idx];
    }

    Mcmcan_LogProfileTableRxCompact(g_mcmcan.rxMsg.messageId, rxData);

    return TRUE;
}

uint32 Mcmcan_GetLastRxMessageId(void)
{
    return g_mcmcan.rxMsg.messageId;
}
