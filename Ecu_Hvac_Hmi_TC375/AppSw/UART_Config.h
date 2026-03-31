#ifndef UART_CONFIG_H
#define UART_CONFIG_H

#include "Ifx_Types.h"
#include "IfxAsclin_Asc.h"

#define UART_TX_BUFFER_SIZE 256
#define UART_RX_BUFFER_SIZE 256

void init_UART(void);
void UART_Printf(const char *fmt, ...);

#endif
