/*********************************************************************************************************************/
/*-----------------------------------------------------Includes------------------------------------------------------*/
/*********************************************************************************************************************/
#include "Base_Com_Qspi1.h"
#include "IfxCpu.h"
#include "IfxQspi_SpiMaster.h"
#include "IfxPort.h"
#include "IfxScuWdt.h"
#include "Bsp.h"
#include "IfxStm.h"

/*********************************************************************************************************************/
/*------------------------------------------------------Macros-------------------------------------------------------*/
/*********************************************************************************************************************/
/* ---------------- Pin mapping ----------------
 * QSPI1 SCLK : P10.2
 * QSPI1 MRST : P10.1
 * QSPI1 MTSR : P10.3
 * RC522 CS   : P10.5 (manual GPIO)
 * RC522 RST  : P02.7 (manual GPIO)
 * -------------------------------------------- */

#define BASE_COM_QSPI1_MODULE                (&MODULE_QSPI1)

#define BASE_COM_QSPI1_MASTER_CHANNEL_BAUDRATE    (100000U)

/* Interrupt Service Routine priorities for Master SPI communication */
#define BASE_COM_QSPI1_ISR_PRIORITY_MASTER_TX     (10U)
#define BASE_COM_QSPI1_ISR_PRIORITY_MASTER_RX     (11U)
#define BASE_COM_QSPI1_ISR_PRIORITY_MASTER_ER     (12U)

#define BASE_COM_QSPI1_WAIT_US(us) \
    waitTime(IfxStm_getTicksFromMicroseconds(BSP_DEFAULT_TIMER, (us)))

#define BASE_COM_QSPI1_WAIT_MS(ms) \
    waitTime(IfxStm_getTicksFromMilliseconds(BSP_DEFAULT_TIMER, (ms)))

/*********************************************************************************************************************/
/*-------------------------------------------------Global variables--------------------------------------------------*/
/*********************************************************************************************************************/
Base_Com_Qspi1_Type g_base_com_qspi1;

/*********************************************************************************************************************/
/*----------------------------------------------Function Implementations---------------------------------------------*/
/*********************************************************************************************************************/
IFX_INTERRUPT(Base_Com_Qspi1_Master_TxIsr, 0, BASE_COM_QSPI1_ISR_PRIORITY_MASTER_TX);
IFX_INTERRUPT(Base_Com_Qspi1_Master_RxIsr, 0, BASE_COM_QSPI1_ISR_PRIORITY_MASTER_RX);
IFX_INTERRUPT(Base_Com_Qspi1_Master_ErIsr, 0, BASE_COM_QSPI1_ISR_PRIORITY_MASTER_ER);

void Base_Com_Qspi1_Master_TxIsr(void)
{
    IfxCpu_enableInterrupts();
    IfxQspi_SpiMaster_isrTransmit(&g_base_com_qspi1.spiMaster);
}

void Base_Com_Qspi1_Master_RxIsr(void)
{
    IfxCpu_enableInterrupts();
    IfxQspi_SpiMaster_isrReceive(&g_base_com_qspi1.spiMaster);
}

void Base_Com_Qspi1_Master_ErIsr(void)
{
    IfxCpu_enableInterrupts();
    IfxQspi_SpiMaster_isrError(&g_base_com_qspi1.spiMaster);
}

void Base_Com_Qspi1_Master_Init(void)
{
    IfxQspi_SpiMaster_Config spiMasterConfig;

    IfxQspi_SpiMaster_initModuleConfig(&spiMasterConfig, BASE_COM_QSPI1_MODULE);

    spiMasterConfig.mode = IfxQspi_Mode_master;

    /* Select the port pins for communication */
    const IfxQspi_SpiMaster_Pins baseComQspi1MasterPins =
    {
        &IfxQspi1_SCLK_P10_2_OUT,
        IfxPort_OutputMode_pushPull,
        &IfxQspi1_MTSR_P10_3_OUT,
        IfxPort_OutputMode_pushPull,
        &IfxQspi1_MRSTA_P10_1_IN,
        IfxPort_InputMode_pullDown,
        IfxPort_PadDriver_cmosAutomotiveSpeed1
    };
    spiMasterConfig.pins = &baseComQspi1MasterPins;

    /* Set the ISR priorities and the service provider */
    spiMasterConfig.txPriority  = BASE_COM_QSPI1_ISR_PRIORITY_MASTER_TX;
    spiMasterConfig.rxPriority  = BASE_COM_QSPI1_ISR_PRIORITY_MASTER_RX;
    spiMasterConfig.erPriority  = BASE_COM_QSPI1_ISR_PRIORITY_MASTER_ER;
    spiMasterConfig.isrProvider = IfxSrc_Tos_cpu0;

    /* Initialize the QSPI Master module */
    IfxQspi_SpiMaster_initModule(&g_base_com_qspi1.spiMaster, &spiMasterConfig);
}

void Base_Com_Qspi1_Master_Channel_Init(void)
{
    IfxQspi_SpiMaster_ChannelConfig spiMasterChannelConfig;

    /* Initialize the configuration with default values */
    IfxQspi_SpiMaster_initChannelConfig(&spiMasterChannelConfig, &g_base_com_qspi1.spiMaster);

    /* RC522:
     * CPOL = 0, CPHA = 0 equivalent setting
     * - idleLow
     * - sample on rising edge
     * - change data on falling edge
     */
    spiMasterChannelConfig.ch.baudrate           = BASE_COM_QSPI1_MASTER_CHANNEL_BAUDRATE;
    spiMasterChannelConfig.ch.mode.dataWidth     = 8;
    spiMasterChannelConfig.ch.mode.clockPolarity = IfxQspi_ClockPolarity_idleLow;
    spiMasterChannelConfig.ch.mode.shiftClock    = IfxQspi_ShiftClock_shiftTransmitDataOnTrailingEdge;

    /* Select the port pin for the Chip Select signal */
    const IfxQspi_SpiMaster_Output baseComQspi1SlaveSelect =
    {
        &IfxQspi1_SLSO9_P10_5_OUT,
        IfxPort_OutputMode_pushPull,
        IfxPort_PadDriver_cmosAutomotiveSpeed1
    };
    spiMasterChannelConfig.sls.output = baseComQspi1SlaveSelect;

    /* Initialize the QSPI Master channel */
    IfxQspi_SpiMaster_initChannel(&g_base_com_qspi1.spiMasterChannel, &spiMasterChannelConfig);
}

void Base_Com_Qspi1_Master_Buffers_Init(void)
{
    uint8 i;

    for (i = 0U; i < BASE_COM_QSPI1_BUFFER_SIZE; i++)
    {
        g_base_com_qspi1.spiBuffers.spiMasterTxBuffer[i] = 0U;
        g_base_com_qspi1.spiBuffers.spiMasterRxBuffer[i] = 0U;
    }
}

void Base_Com_Qspi1_Init(void)
{
    Base_Com_Qspi1_Master_Init();
    Base_Com_Qspi1_Master_Channel_Init();
    Base_Com_Qspi1_Master_Buffers_Init();
}

uint8 Base_Com_Qspi1_TransferByte(uint8 tx)
{
    while (IfxQspi_SpiMaster_getStatus(&g_base_com_qspi1.spiMasterChannel) == IfxQspi_Status_busy)
    {
    }

    g_base_com_qspi1.spiBuffers.spiMasterTxBuffer[0] = tx;
    g_base_com_qspi1.spiBuffers.spiMasterRxBuffer[0] = 0U;

    IfxQspi_SpiMaster_exchange(&g_base_com_qspi1.spiMasterChannel,
                               &g_base_com_qspi1.spiBuffers.spiMasterTxBuffer[0],
                               &g_base_com_qspi1.spiBuffers.spiMasterRxBuffer[0],
                               1);

    while (IfxQspi_SpiMaster_getStatus(&g_base_com_qspi1.spiMasterChannel) == IfxQspi_Status_busy)
    {
    }

    return g_base_com_qspi1.spiBuffers.spiMasterRxBuffer[0];
}

void Base_Com_Qspi1_Transfer(const uint8 *tx, uint8 *rx, uint32 len)
{
    uint32 i;

    if ((tx == NULL_PTR) || (rx == NULL_PTR) || (len == 0U) || (len > BASE_COM_QSPI1_BUFFER_SIZE))
    {
        return;
    }

    while (IfxQspi_SpiMaster_getStatus(&g_base_com_qspi1.spiMasterChannel) == IfxQspi_Status_busy)
    {
    }

    for (i = 0U; i < len; ++i)
    {
        g_base_com_qspi1.spiBuffers.spiMasterTxBuffer[i] = tx[i];
        g_base_com_qspi1.spiBuffers.spiMasterRxBuffer[i] = 0U;
    }

    IfxQspi_SpiMaster_exchange(&g_base_com_qspi1.spiMasterChannel,
                               g_base_com_qspi1.spiBuffers.spiMasterTxBuffer,
                               g_base_com_qspi1.spiBuffers.spiMasterRxBuffer,
                               (sint16)len);

    while (IfxQspi_SpiMaster_getStatus(&g_base_com_qspi1.spiMasterChannel) == IfxQspi_Status_busy)
    {
    }

    for (i = 0U; i < len; ++i)
    {
        rx[i] = g_base_com_qspi1.spiBuffers.spiMasterRxBuffer[i];
    }
}
