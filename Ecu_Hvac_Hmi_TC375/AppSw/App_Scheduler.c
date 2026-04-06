/*********************************************************************************************************************/
/*-----------------------------------------------------Includes------------------------------------------------------*/
/*********************************************************************************************************************/
#include "App_Scheduler.h"

#include "App_Can_Service.h"
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
#include "UART_Config.h"

/*********************************************************************************************************************/
/*------------------------------------------------------Macros-------------------------------------------------------*/
/*********************************************************************************************************************/

/*********************************************************************************************************************/
/*--------------------------------------------Private Variables/Constants--------------------------------------------*/
/*********************************************************************************************************************/
// Shared_Profile_Table_t proftable;
// boolean proftable_updated = FALSE;
// uint8 activatedProfileIndex = 0;
// Shared_System_State_t systemState;

static App_Manager_System_Input_t g_app_scheduler_system_input;
static App_Manager_System_Output_t g_app_scheduler_system_output;

static boolean g_tx_state_requested = FALSE;
static boolean g_tx_temp_requested = FALSE;
static boolean g_app_scheduler_profile_table_tx_requested = FALSE;
/*********************************************************************************************************************/
/*------------------------------------------------Function Prototypes------------------------------------------------*/
/*********************************************************************************************************************/
static void App_Scheduler_Run_1ms(void);
static void App_Scheduler_Run_10ms(void);
static void App_Scheduler_Run_100ms(void);
static void App_Scheduler_Run_1s(void);
void App_Scheduler_Init(void);
void App_Scheduler_Run(void);

/* CAN */
static void App_Scheduler_Task_CanRx(void);
static void App_Scheduler_Task_CanTx(void);

/*********************************************************************************************************************/
/*---------------------------------------------Function Implementations----------------------------------------------*/
/*********************************************************************************************************************/
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
  App_Scheduler_Task_CanRx();
  App_Manager_HVAC_Run();
  App_Scheduler_Task_CanTx();
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

/*********************************************************************************************************************/
/*------------------------------------------------CAN Task Functions-------------------------------------------------*/
/*********************************************************************************************************************/
static void App_Scheduler_Task_CanRx(void)
{
  Shared_Can_Frame_t rx_frame;

  App_Can_Service_ResetSystemInput(&g_app_scheduler_system_input);

  while (App_Can_Service_ReadFrame(&rx_frame) == TRUE)
  {
    App_Can_Service_HandleRxFrame(&rx_frame,
                                  &g_app_scheduler_system_input);
  }
}

static void App_Scheduler_Task_CanTx(void)
{
  Shared_Can_Frame_t tx_frame;

  if (g_app_scheduler_profile_table_tx_requested == TRUE)
  {
    if (App_Can_Service_BuildProfileTableFrame(&g_app_scheduler_system_output.profile_table,
                                               &tx_frame) == TRUE)
    {
      (void)App_Can_Service_WriteFrame(&tx_frame);
      UART_Printf("[TX] SS_PROFILE_TABLE sent\r\n");
    }

    g_app_scheduler_profile_table_tx_requested = FALSE;
  }

  if (g_tx_state_requested == TRUE)
  {
    if (App_Can_Service_BuildStateFrame(
            (Shared_System_State_t)g_app_scheduler_system_output.current_state,
            &tx_frame) == TRUE)
    {
      (void)App_Can_Service_WriteFrame(&tx_frame);
      UART_Printf("[TX] SS_STATE sent\r\n");
    }

    g_tx_state_requested = FALSE;
  }

  if (g_tx_temp_requested == TRUE)
  {
    if (App_Can_Service_BuildTempFrame(g_app_scheduler_system_output.temperature,
                                       &tx_frame) == TRUE)
    {
      (void)App_Can_Service_WriteFrame(&tx_frame);
      UART_Printf("[TX] SS_TEMP sent\r\n");
    }

    g_tx_temp_requested = FALSE;
  }
}