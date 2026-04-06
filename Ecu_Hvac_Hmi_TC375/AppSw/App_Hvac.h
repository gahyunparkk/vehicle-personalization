#ifndef HVAC_H_
#define HVAC_H_

#include "Ifx_Types.h"

void App_Manaver_HVAC_Init(void);

// 온도 설정 함수
uint8 Hvac_setHeatThreshold(sint8 th);
uint8 Hvac_getHeatThreshold(void);
uint8 Hvac_setCoolThreshold(sint8 th);
uint8 Hvac_getCoolThreshold(void);
void App_Manager_Hvac_updateTemp(sint8 temp);

// 주기적으로 실행해야 할 함수
void App_Manager_HVAC_Run(void);

#endif