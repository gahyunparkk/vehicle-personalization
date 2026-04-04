#ifndef BASE_BUZZER_H_
#define BASE_BUZZER_H_

#include "Ifx_Types.h"

void initPwm_Tout107(void);
void updatePwm10_Duty(uint32 dutyTicks);
void startPwm10(void);
void stopPwm10(void);
void turnOffPwm10_Soft(void);
void setPwm10_FreqAndDuty(uint32 freqHz, float32 dutyPercent);

#endif