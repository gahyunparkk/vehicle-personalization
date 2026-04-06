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

static uint32 g_app_scheduler_now_ms = 0u;
static boolean g_tx_idx_requested = FALSE;
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
void App_Scheduler_IdxTxReq(void);
void App_Scheduler_TableTxReq(void);

/* CAN */
static void App_Scheduler_Task_CanRx(void);
static void App_Scheduler_Task_CanTx(void);

static void App_Scheduler_Task_System(void);

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
  App_Scheduler_Task_System();
  App_Manager_HVAC_Run();
  App_Scheduler_Task_CanTx();
}

static void App_Scheduler_Run_1s(void)
{
}

void App_Scheduler_Init(void)
{
  init_UART();
  Driver_Stm_Init();
  App_Manager_UI_Init();
  App_Manaver_HVAC_Init();
  initMcmcan();
  App_Manager_System_Init();
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

  while (App_Can_Service_ReadFrame(&rx_frame) == TRUE)
  {
    App_Can_Service_HandleRxFrame(&rx_frame);
  }
}

static void App_Scheduler_Task_CanTx(void)
{
  Shared_Can_Frame_t tx_frame;
  uint8 pf;
  Shared_Profile_Table_t pft;
  App_Manager_System_GetProfileTable(&pft);
  if (g_app_scheduler_profile_table_tx_requested == TRUE)
  {
    if (App_Can_Service_BuildProfileTableFrame(&pft, &tx_frame) == TRUE)
    {
      (void)App_Can_Service_WriteFrame(&tx_frame);
      UART_Printf("[TX] SS_PROFILE_TABLE sent\r\n");
    }

    g_app_scheduler_profile_table_tx_requested = FALSE;
  }

  if (g_tx_idx_requested == TRUE)
  {
    App_Manager_System_GetActiveProfileIndex(&pf);
    if (App_Can_Service_BuildStateFrame(pf, &tx_frame) == TRUE)
    {
      (void)App_Can_Service_WriteFrame(&tx_frame);
      UART_Printf("[TX] HH_IDX sent\r\n");
    }

    g_tx_idx_requested = FALSE;
  }
}

static void App_Scheduler_Task_System(void)
{
}

void App_Scheduler_IdxTxReq(void)
{
  g_tx_idx_requested = TRUE;
}

void App_Scheduler_TableTxReq(void)
{
  g_app_scheduler_profile_table_tx_requested = TRUE;
}
