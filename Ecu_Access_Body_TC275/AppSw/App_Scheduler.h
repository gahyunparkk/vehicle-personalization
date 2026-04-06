#ifndef APP_SCHEDULER_H
#define APP_SCHEDULER_H

#include "Ifx_Types.h"

void App_Init(void);
void AppScheduling(void);

void AppTask1ms(void);
void AppTask10ms(void);
void AppTask100ms(void);
void AppTask1000ms(void);

#endif
