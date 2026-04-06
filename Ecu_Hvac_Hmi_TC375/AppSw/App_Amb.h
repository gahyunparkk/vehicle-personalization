#ifndef APP_AMB_H_
#define APP_AMB_H_

#include "Ifx_Types.h"

#ifndef AMB_MODE_E
#define AMB_MODE_E
typedef enum
{
  AMB_OFF,
  AMB_CONSTANT,
  AMB_BREATH,
  AMB_WAVE_L,
  AMB_WAVE_R,
  AMB_BLINK
} Amb_mode_e;
#endif

void App_Manager_Ambient_Init(void);
void App_Ambient_Nextmode(void);
void App_Ambient_changeColor(sint8 amount);
void Amb_getmode(Amb_mode_e *mode);
void Amb_setmode(Amb_mode_e mode);
void Amb_setcolor2x(uint8 amount);
void Amb_off(void);
void Amb_on(void);
void Amb_getHue(uint16 *hue);

// 일정 주기로 실행 필요
void App_Manager_Ambient_Run(void);

#endif
