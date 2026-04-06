#include "App_Scheduler.h"

#include "App_Hvac.h"
#include "App_LCD.h"
#include "App_Scheduler.h"
#include "App_UI.h"

#include "Shared_Can_Message.h"
#include "Shared_Profile.h"
#include "Shared_System_State.h"
#include "Shared_Util_Time.h"

#include "Driver_Stm.h"
#include "MCMCAN_FD.h"

static void App_Scheduler_Run_1ms(void)
{
}

static void App_Scheduler_Run_10ms(void)
{
  App_Manager_UI_Run();
  App_Manager_Ambient_Run();
}

static void App_Scheduler_Run_100ms(void)
{
  App_Manager_HVAC_Run();
}

static void App_Scheduler_Run_1s(void)
{
}

void App_Scheduler_Init(void)
{
  Driver_Stm_Init();
  App_Manager_UI_Init();
  App_Manaver_HVAC_Init();
}

void App_Scheduler_Run(void)
{
  if (stSchedulingInfo.u8nuScheduling1msFlag == 1u)
  {
    stSchedulingInfo.u8nuScheduling1msFlag = 0u;
    App_Scheduler_Run_1ms();

    if (stSchedulingInfo.u8nuScheduling10msFlag == 1u)
    {
      stSchedulingInfo.u8nuScheduling10msFlag = 0u;
      App_Scheduler_Run_10ms();
    }

    if (stSchedulingInfo.u8nuScheduling100msFlag == 1u)
    {
      stSchedulingInfo.u8nuScheduling100msFlag = 0u;
      App_Scheduler_Run_100ms();
    }

    if (stSchedulingInfo.u8nuScheduling1000msFlag == 1u)
    {
      stSchedulingInfo.u8nuScheduling1000msFlag = 0u;
      App_Scheduler_Run_1s();
    }
  }
}
