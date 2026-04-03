#ifndef BASE_COM_QSPI1_H_
#define BASE_COM_QSPI1_H_

/*********************************************************************************************************************/
/*-----------------------------------------------------Includes------------------------------------------------------*/
/*********************************************************************************************************************/
#include "Ifx_Types.h"
#include "IfxQspi_SpiMaster.h"
#include "IfxQspi_SpiSlave.h"

/*********************************************************************************************************************/
/*------------------------------------------------------Macros-------------------------------------------------------*/
/*********************************************************************************************************************/
#define BASE_COM_QSPI1_BUFFER_SIZE    128U    /* Buffer size */

/*********************************************************************************************************************/
/*-------------------------------------------------Data Structures---------------------------------------------------*/
/*********************************************************************************************************************/
typedef struct
{
    uint8 spiMasterTxBuffer[BASE_COM_QSPI1_BUFFER_SIZE];    /* QSPI master transmit buffer */
    uint8 spiMasterRxBuffer[BASE_COM_QSPI1_BUFFER_SIZE];    /* QSPI master receive buffer  */
    uint8 spiSlaveTxBuffer[BASE_COM_QSPI1_BUFFER_SIZE];     /* QSPI slave transmit buffer  */
    uint8 spiSlaveRxBuffer[BASE_COM_QSPI1_BUFFER_SIZE];     /* QSPI slave receive buffer   */
} Base_Com_Qspi1_BuffersType;

typedef struct
{
    Base_Com_Qspi1_BuffersType spiBuffers;       /* Buffer instance              */
    IfxQspi_SpiMaster          spiMaster;        /* QSPI master handle           */
    IfxQspi_SpiMaster_Channel  spiMasterChannel; /* QSPI master channel handle   */
    IfxQspi_SpiSlave           spiSlave;         /* QSPI slave handle            */
} Base_Com_Qspi1_Type;

/*********************************************************************************************************************/
/*-------------------------------------------------Global Variables--------------------------------------------------*/
/*********************************************************************************************************************/
extern Base_Com_Qspi1_Type g_base_com_qspi1;

/*********************************************************************************************************************/
/*------------------------------------------------Function Prototypes------------------------------------------------*/
/*********************************************************************************************************************/
void  Base_Com_Qspi1_Init(void);
uint8 Base_Com_Qspi1_TransferByte(uint8 tx);
void  Base_Com_Qspi1_Transfer(const uint8 *tx, uint8 *rx, uint32 len);

#endif /* BASE_COM_QSPI1_H_ */
