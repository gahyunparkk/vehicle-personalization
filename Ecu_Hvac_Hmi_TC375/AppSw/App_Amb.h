#ifndef APP_AMB_H_
#define APP_AMB_H_

#include "Ifx_Types.h"

#ifndef AMB_MODE_E
#define AMB_MODE_E
typedef enum
{
  AMB_CONSTANT,
  AMB_BREATH,
  AMB_WAVE_L,
  AMB_WAVE_R
} Amb_mode_e;
#endif

void App_Manager_Ambient_Init(void);
void Amb_nextmode(void);
void Amb_changeColor(sint8 amount);
Amb_mode_e Amb_getmode(void);
void Amb_off(void);
void Amb_on(void);

// 일정 주기로 실행 필요
void App_Manager_Ambient_Run(void);

#endif
