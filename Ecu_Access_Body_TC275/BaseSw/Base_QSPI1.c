/*********************************************************************************************************************/
/*-----------------------------------------------------Includes------------------------------------------------------*/
/*********************************************************************************************************************/
#include "Base_QSPI1.h"
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

#define QSPI1_MASTER       &MODULE_QSPI1

#define MASTER_CHANNEL_BAUDRATE     100000U         /* Master channel baud rate                                     */

/* Interrupt Service Routine priorities for Master and Slave SPI communication */
#define ISR_PRIORITY_MASTER_TX      50
#define ISR_PRIORITY_MASTER_RX      51
#define ISR_PRIORITY_MASTER_ER      52

/*********************************************************************************************************************/
/*-------------------------------------------------Global variables--------------------------------------------------*/
/*********************************************************************************************************************/
qspiComm g_qspi1;

/*********************************************************************************************************************/
/*----------------------------------------------Function Implementations---------------------------------------------*/
/*********************************************************************************************************************/
#define QSPI1_WAIT_US(us)  waitTime(IfxStm_getTicksFromMicroseconds(BSP_DEFAULT_TIMER, (us)))
#define QSPI1_WAIT_MS(ms)  waitTime(IfxStm_getTicksFromMilliseconds(BSP_DEFAULT_TIMER, (ms)))

IFX_INTERRUPT(QSPI1_Master_TxISR, 0, ISR_PRIORITY_MASTER_TX);
IFX_INTERRUPT(QSPI1_Master_RxISR, 0, ISR_PRIORITY_MASTER_RX);
IFX_INTERRUPT(QSPI1_Master_ErISR, 0, ISR_PRIORITY_MASTER_ER);

void QSPI1_Master_TxISR()
{
    IfxCpu_enableInterrupts();
    IfxQspi_SpiMaster_isrTransmit(&g_qspi1.spiMaster);
}


void QSPI1_Master_RxISR()
{
    IfxCpu_enableInterrupts();
    IfxQspi_SpiMaster_isrReceive(&g_qspi1.spiMaster);
}


void QSPI1_Master_ErISR()
{
    IfxCpu_enableInterrupts();
    IfxQspi_SpiMaster_isrError(&g_qspi1.spiMaster);
}


void QSPI1_Master_Init(void)
{
    IfxQspi_SpiMaster_Config spiMasterConfig;                           /* Define a Master configuration            */

    IfxQspi_SpiMaster_initModuleConfig(&spiMasterConfig, QSPI1_MASTER); /* Initialize it with default values        */

    spiMasterConfig.mode =        IfxQspi_Mode_master;                         /* Configure the mode                       */

    /* Select the port pins for communication */
    const IfxQspi_SpiMaster_Pins qspi1MasterPins = {
        &IfxQspi1_SCLK_P10_2_OUT, IfxPort_OutputMode_pushPull,          /* SCLK Pin                       (CLK)     */
        &IfxQspi1_MTSR_P10_3_OUT, IfxPort_OutputMode_pushPull,          /* MasterTransmitSlaveReceive pin (MOSI)    */
        &IfxQspi1_MRSTA_P10_1_IN, IfxPort_InputMode_pullDown,           /* MasterReceiveSlaveTransmit pin (MISO)    */
        IfxPort_PadDriver_cmosAutomotiveSpeed1                          /* Pad driver mode                          */
    };
    spiMasterConfig.pins = &qspi1MasterPins;                            /* Assign the Master's port pins            */

    /* Set the ISR priorities and the service provider */
    spiMasterConfig.txPriority = ISR_PRIORITY_MASTER_TX;
    spiMasterConfig.rxPriority = ISR_PRIORITY_MASTER_RX;
    spiMasterConfig.erPriority = ISR_PRIORITY_MASTER_ER;
    spiMasterConfig.isrProvider = IfxSrc_Tos_cpu0;

    /* Initialize the QSPI Master module */
    IfxQspi_SpiMaster_initModule(&g_qspi1.spiMaster, &spiMasterConfig);
}


void QSPI1_Master_Channel_Init(void) {
    IfxQspi_SpiMaster_ChannelConfig spiMasterChannelConfig;             /* Define a Master Channel configuration    */

    /* Initialize the configuration with default values */
    IfxQspi_SpiMaster_initChannelConfig(&spiMasterChannelConfig, &g_qspi1.spiMaster);

    /* 클럭의 trailing edge에서 데이터 바꾸기.
     * CPOL=0이므로 leading: rising, trailing: falling
     * RC522는 rising edge에서 샘플링, falling edge에서 값을 바꿈
     * 따라서 shiftClock은 fallingEdge
     */
    spiMasterChannelConfig.ch.baudrate           = MASTER_CHANNEL_BAUDRATE;
    spiMasterChannelConfig.ch.mode.dataWidth     = 8;
    spiMasterChannelConfig.ch.mode.clockPolarity = IfxQspi_ClockPolarity_idleLow;
    spiMasterChannelConfig.ch.mode.shiftClock    = IfxQspi_ShiftClock_shiftTransmitDataOnTrailingEdge;

    /* Select the port pin for the Chip Select signal */
    const IfxQspi_SpiMaster_Output qspi1SlaveSelect = {                 /* QSPI1 Master selects the QSPI3 Slave     */
        &IfxQspi1_SLSO9_P10_5_OUT, IfxPort_OutputMode_pushPull,         /* Slave Select port pin (CS)               */
        IfxPort_PadDriver_cmosAutomotiveSpeed1                          /* Pad driver mode                          */
    };
    spiMasterChannelConfig.sls.output = qspi1SlaveSelect;

    /* Initialize the QSPI Master channel */
    IfxQspi_SpiMaster_initChannel(&g_qspi1.spiMasterChannel, &spiMasterChannelConfig);
}


/* QSPI Master SW buffer initialization
 * This function initializes SW buffers the Master uses.
 */
void QSPI1_Master_Buffers_Init(void)
{
    for (uint8 i = 0; i < SPI_BUFFER_SIZE; i++)
    {
        g_qspi1.spiBuffers.spiMasterTxBuffer[i] = 0;
        g_qspi1.spiBuffers.spiMasterRxBuffer[i] = 0;                     /* Clear RX Buffer                          */
    }
}


void QSPI1_Init(void) {
    QSPI1_Master_Init();
    QSPI1_Master_Channel_Init();
    QSPI1_Master_Buffers_Init();
}


uint8 QSPI1_TransferByte(uint8 tx)
{
    while (IfxQspi_SpiMaster_getStatus(&g_qspi1.spiMasterChannel) == IfxQspi_Status_busy)
    {
    }

    g_qspi1.spiBuffers.spiMasterTxBuffer[0] = tx;
    g_qspi1.spiBuffers.spiMasterRxBuffer[0] = 0U;

    IfxQspi_SpiMaster_exchange(&g_qspi1.spiMasterChannel,
                               &g_qspi1.spiBuffers.spiMasterTxBuffer[0],
                               &g_qspi1.spiBuffers.spiMasterRxBuffer[0],
                               1);

    while (IfxQspi_SpiMaster_getStatus(&g_qspi1.spiMasterChannel) == IfxQspi_Status_busy)
    {
    }

    return g_qspi1.spiBuffers.spiMasterRxBuffer[0];
}


void QSPI1_Transfer(const uint8 *tx, uint8 *rx, uint32 len)
{
    uint32 i;

    if ((tx == NULL_PTR) || (rx == NULL_PTR) || (len == 0U) || (len > SPI_BUFFER_SIZE))
    {
        return;
    }

    while (IfxQspi_SpiMaster_getStatus(&g_qspi1.spiMasterChannel) == IfxQspi_Status_busy)
    {
    }

    for (i = 0U; i < len; ++i)
    {
        g_qspi1.spiBuffers.spiMasterTxBuffer[i] = tx[i];
        g_qspi1.spiBuffers.spiMasterRxBuffer[i] = 0U;
    }

    IfxQspi_SpiMaster_exchange(&g_qspi1.spiMasterChannel,
                               g_qspi1.spiBuffers.spiMasterTxBuffer,
                               g_qspi1.spiBuffers.spiMasterRxBuffer,
                               len);

    while (IfxQspi_SpiMaster_getStatus(&g_qspi1.spiMasterChannel) == IfxQspi_Status_busy)
    {
    }

    for (i = 0U; i < len; ++i)
    {
        rx[i] = g_qspi1.spiBuffers.spiMasterRxBuffer[i];
    }
}
