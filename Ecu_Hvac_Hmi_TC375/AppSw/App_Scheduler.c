#include "App_Scheduler.h"

#include "App_Hvac.h"
#include "App_LCD.h"
#include "App_Scheduler.h"
#include "App_UI.h"

#include "Driver_Stm.h"

static void AppTask1ms(void)
{
}

static void AppTask10ms(void)
{
  UI_task();
  Amb_transition();
}

static void AppTask100ms(void)
{
  Hvac_updateHvac();
}

static void AppTask1000ms(void)
{
}

void Systeminit(void)
{
  Driver_Stm_Init();
  UI_init();
  Hvac_init();
}

void AppScheduling(void)
{
  if (stSchedulingInfo.u8nuScheduling1msFlag == 1u)
  {
    stSchedulingInfo.u8nuScheduling1msFlag = 0u;
    AppTask1ms();

    if (stSchedulingInfo.u8nuScheduling10msFlag == 1u)
    {
      stSchedulingInfo.u8nuScheduling10msFlag = 0u;
      AppTask10ms();
    }

    if (stSchedulingInfo.u8nuScheduling100msFlag == 1u)
    {
      stSchedulingInfo.u8nuScheduling100msFlag = 0u;
      AppTask100ms();
    }

    if (stSchedulingInfo.u8nuScheduling1000msFlag == 1u)
    {
      stSchedulingInfo.u8nuScheduling1000msFlag = 0u;
      AppTask1000ms();
    }
  }
}
