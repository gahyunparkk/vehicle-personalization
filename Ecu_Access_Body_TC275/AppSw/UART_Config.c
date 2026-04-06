#include "UART_Config.h"
#include "IfxAsclin_Asc.h"
#include "IfxCpu_Irq.h"
#include "IfxPort.h"
#include <stdarg.h>
#include <stdio.h>

#define ISR_PRIORITY_ASCLIN3_TX  20
#define ISR_PRIORITY_ASCLIN3_RX  21
#define ISR_PRIORITY_ASCLIN3_ER  22

IfxAsclin_Asc g_asc;

/* iLLD가 요구하는 정렬된 버퍼 */
static uint8 g_txBuffer[UART_TX_BUFFER_SIZE + sizeof(Ifx_Fifo) + 8];
static uint8 g_rxBuffer[UART_RX_BUFFER_SIZE + sizeof(Ifx_Fifo) + 8];

/* ── ISR ── */
IFX_INTERRUPT(asclin3TxISR, 0, ISR_PRIORITY_ASCLIN3_TX)
{
    IfxAsclin_Asc_isrTransmit(&g_asc);
}

IFX_INTERRUPT(asclin3RxISR, 0, ISR_PRIORITY_ASCLIN3_RX)
{
    IfxAsclin_Asc_isrReceive(&g_asc);
}

IFX_INTERRUPT(asclin3ErISR, 0, ISR_PRIORITY_ASCLIN3_ER)
{
    IfxAsclin_Asc_isrError(&g_asc);
}

void init_UART(void)
{
    IfxAsclin_Asc_Config ascConfig;
    IfxAsclin_Asc_initModuleConfig(&ascConfig, &MODULE_ASCLIN3);

    ascConfig.baudrate.baudrate = 115200;
    ascConfig.baudrate.oversampling = IfxAsclin_OversamplingFactor_16;

    ascConfig.interrupt.txPriority = ISR_PRIORITY_ASCLIN3_TX;
    ascConfig.interrupt.rxPriority = ISR_PRIORITY_ASCLIN3_RX;
    ascConfig.interrupt.erPriority = ISR_PRIORITY_ASCLIN3_ER;
    ascConfig.interrupt.typeOfService = IfxSrc_Tos_cpu0;

    ascConfig.txBuffer = g_txBuffer;
    ascConfig.txBufferSize = UART_TX_BUFFER_SIZE;
    ascConfig.rxBuffer = g_rxBuffer;
    ascConfig.rxBufferSize = UART_RX_BUFFER_SIZE;

    static const IfxAsclin_Asc_Pins pins = {
        NULL_PTR,                  IfxPort_InputMode_pullUp,
        &IfxAsclin3_RXD_P32_2_IN,  IfxPort_InputMode_pullUp,
        NULL_PTR,                  IfxPort_OutputMode_pushPull,
        &IfxAsclin3_TX_P15_7_OUT,  IfxPort_OutputMode_pushPull,
        IfxPort_PadDriver_cmosAutomotiveSpeed1
    };
    ascConfig.pins = &pins;

    IfxAsclin_Asc_initModule(&g_asc, &ascConfig);
}

void UART_SendByte(uint8 data)
{
    IfxAsclin_Asc_blockingWrite(&g_asc, data);
}

void UART_Printf(const char *fmt, ...)
{
    char buf[128];
    va_list ap;
    va_start(ap, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    if (len < 0) return;
    if (len >= (int)sizeof(buf)) len = sizeof(buf) - 1;

    for (int i = 0; i < len; ++i)
    {
        UART_SendByte((uint8)buf[i]);
    }
}
