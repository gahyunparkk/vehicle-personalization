#ifndef HVAC_H_
#define HVAC_H_

#include "Ifx_Types.h"

void Hvac_init(void);

// 온도 설정 함수
uint8 Hvac_setHeatThreshold(uint8 th);
uint8 Hvac_getHeatThreshold(void);
uint8 Hvac_setCoolThreshold(uint8 th);
uint8 Hvac_getCoolThreshold(void);

// 주기적으로 실행해야 할 함수
void Hvac_updateHvac(void);

#endif